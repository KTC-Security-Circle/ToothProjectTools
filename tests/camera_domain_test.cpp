#include "cmd/commands.hpp"
#include "control/control_message.hpp"
#include "handler/camera_command_handler.hpp"
#include "handler/projector_command_handler.hpp"
#include "handler/window_resource_command_handler.hpp"
#include "headless/headless_command_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/camera_service.hpp"
#include "service/monitor_service.hpp"
#include "service/projector_service.hpp"
#include "service/window_service.hpp"
#include "video/camera_manager.hpp"

#include <cassert>
#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <variant>

namespace
{

class FakeWindowBackend final : public service::window::WindowBackend
{
  public:
    win::WindowId openWindow(const std::string& title, int width, int height,
                             std::optional<int> monitor_index, bool fullscreen) override
    {
        last_title = title;
        last_width = width;
        last_height = height;
        last_monitor_index = monitor_index;
        last_fullscreen = fullscreen;
        opened = true;
        return next_id++;
    }

    bool closeWindow(win::WindowId window_id) override
    {
        last_closed_id = window_id;
        opened = false;
        ++close_count;
        return close_result;
    }

    bool showImage(win::WindowId window_id, const cv::Mat& image) override
    {
        last_shown_id = window_id;
        last_shown_size = image.size();
        last_image = image.clone();
        ++show_count;
        return show_result;
    }

    bool configureWindowSurface(win::WindowId window_id, int monitor_index, int x, int y, int width, int height,
                                bool fullscreen) override
    {
        last_configured_id = window_id;
        last_configured_monitor_index = monitor_index;
        last_configured_x = x;
        last_configured_y = y;
        last_configured_width = width;
        last_configured_height = height;
        last_configured_fullscreen = fullscreen;
        ++configure_count;
        return configure_result;
    }

    void pollEvents(int delay_ms) override
    {
        last_delay_ms = delay_ms;
    }

    win::WindowId next_id{1};
    bool opened{false};
    bool close_result{true};
    bool show_result{true};
    bool configure_result{true};
    std::string last_title;
    int last_width{0};
    int last_height{0};
    std::optional<int> last_monitor_index;
    bool last_fullscreen{false};
    win::WindowId last_closed_id{win::kInvalidWindowId};
    win::WindowId last_shown_id{win::kInvalidWindowId};
    cv::Size last_shown_size{};
    cv::Mat last_image;
    win::WindowId last_configured_id{win::kInvalidWindowId};
    int last_configured_monitor_index{0};
    int last_configured_x{0};
    int last_configured_y{0};
    int last_configured_width{0};
    int last_configured_height{0};
    bool last_configured_fullscreen{false};
    int close_count{0};
    int show_count{0};
    int configure_count{0};
    int last_delay_ms{0};
};



template <typename Function>
auto runWindowRequest(service::window::WindowService& service, Function function)
{
    auto future = std::async(std::launch::async, std::move(function));
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            return future.get();
        }
        service.processPendingRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "window request did not finish");
    return future.get();
}

service::monitor::MonitorService fakeMonitorService()
{
    return service::monitor::MonitorService{[]
                                            {
                                                return std::vector<service::monitor::MonitorInfo>{
                                                    {0, 0, 0, 1920, 1080, true, "primary"},
                                                    {1, 1920, 0, 1920, 1080, false, "projector"},
                                                };
                                            }};
}

control::ControlMessage messageWithId()
{
    control::ControlMessage message;
    message.id = "test";
    return message;
}

void testWindowMapper()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    headless::HeadlessCommandMapper mapper{service};

    auto message = messageWithId();
    message.cmd = "open_window";
    message.width = 640;
    message.height = 480;
    auto result = mapper.mapOpenWindow(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.window_role = "projector";
    message.width.reset();
    result = mapper.mapOpenWindow(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.width = 640;
    message.height.reset();
    result = mapper.mapOpenWindow(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.height = 480;
    message.width = 0;
    result = mapper.mapOpenWindow(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.width = 640;
    message.height = 0;
    result = mapper.mapOpenWindow(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.height = 480;
    message.monitor_index = -1;
    result = mapper.mapOpenWindow(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.monitor_index.reset();
    result = mapper.mapOpenWindow(message);
    assert(result.ok && std::holds_alternative<cmd::CmdOpenWindow>(*result.command));
    const auto open = std::get<cmd::CmdOpenWindow>(*result.command);
    assert(open.title == "projector");
    assert(!open.fullscreen);

    message.title = "Projector";
    message.fullscreen = true;
    message.monitor_index = 1;
    result = mapper.mapOpenWindow(message);
    const auto open_fullscreen = std::get<cmd::CmdOpenWindow>(*result.command);
    assert(open_fullscreen.title == "Projector");
    assert(open_fullscreen.fullscreen);
    assert(open_fullscreen.monitor_index == 1);

    message = messageWithId();
    result = mapper.mapCloseWindow(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.window_role = "projector";
    result = mapper.mapCloseWindow(message);
    assert(result.ok && std::holds_alternative<cmd::CmdCloseWindow>(*result.command));
}

void testProjectorMapper()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    headless::HeadlessCommandMapper mapper{service};

    auto message = messageWithId();
    message.window_role = "projector";
    message.width = 640;
    message.height = 480;
    auto result = mapper.mapOpenProjector(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.projector_role = "projector";
    message.window_role.reset();
    result = mapper.mapOpenProjector(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.window_role = "projector";
    message.width.reset();
    result = mapper.mapOpenProjector(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.width = 640;
    message.height.reset();
    result = mapper.mapOpenProjector(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.height = 480;
    message.width = 0;
    result = mapper.mapOpenProjector(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.width = 640;
    message.height = 0;
    result = mapper.mapOpenProjector(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.height = 480;
    result = mapper.mapOpenProjector(message);
    assert(result.ok && std::holds_alternative<cmd::CmdOpenProjector>(*result.command));

    message = messageWithId();
    result = mapper.mapCloseProjector(message);
    assert(!result.ok && result.error->code == "missing_field");
    message.projector_role = "projector";
    result = mapper.mapCloseProjector(message);
    assert(result.ok && std::holds_alternative<cmd::CmdCloseProjector>(*result.command));

    message = messageWithId();
    result = mapper.mapGeneratePatterns(message);
    assert(!result.ok && result.error->code == "missing_field");
    message.projector_role = "projector";
    result = mapper.mapGeneratePatterns(message);
    assert(result.ok && std::holds_alternative<cmd::CmdGeneratePatterns>(*result.command));

    message = messageWithId();
    message.projector_role = "projector";
    result = mapper.mapProjectorShowPattern(message);
    assert(!result.ok && result.error->code == "missing_field");
    message.index = -1;
    result = mapper.mapProjectorShowPattern(message);
    assert(!result.ok && result.error->code == "invalid_command");
    message.index = 0;
    result = mapper.mapProjectorShowPattern(message);
    assert(result.ok && std::holds_alternative<cmd::CmdProjectorShowPattern>(*result.command));

    result = mapper.mapProjectorNextPattern(message);
    assert(result.ok && std::holds_alternative<cmd::CmdProjectorNextPattern>(*result.command));
    result = mapper.mapProjectorPrevPattern(message);
    assert(result.ok && std::holds_alternative<cmd::CmdProjectorPrevPattern>(*result.command));

    message = messageWithId();
    result = mapper.mapListMonitors(message);
    assert(result.ok && std::holds_alternative<cmd::CmdListMonitors>(*result.command));

    message = messageWithId();
    message.monitor_index = 0;
    message.width = 640;
    message.height = 480;
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.projector_role = "projector";
    message.monitor_index.reset();
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.monitor_index = 0;
    message.width.reset();
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.width = 640;
    message.height.reset();
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.height = 480;
    message.width = 0;
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.width = 640;
    message.height = 0;
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.height = 480;
    message.monitor_index = -1;
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.monitor_index = 0;
    message.placement = "left";
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.placement = "custom";
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.x = 0;
    result = mapper.mapConfigureProjectorSurface(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.y = 0;
    result = mapper.mapConfigureProjectorSurface(message);
    assert(result.ok && std::holds_alternative<cmd::CmdConfigureProjectorSurface>(*result.command));

    message.placement = "center";
    message.x.reset();
    message.y.reset();
    result = mapper.mapConfigureProjectorSurface(message);
    assert(result.ok && std::holds_alternative<cmd::CmdConfigureProjectorSurface>(*result.command));
}

void testMapper()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    headless::HeadlessCommandMapper mapper{service};

    auto message = messageWithId();
    message.role = "left";
    auto result = mapper.mapOpenCamera(message);
    assert(!result.ok && result.error->code == "missing_field");

    message = messageWithId();
    message.camera_id = 0;
    result = mapper.mapOpenCamera(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.role = "left";
    message.camera_id = -1;
    result = mapper.mapOpenCamera(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.camera_id = 0;
    result = mapper.mapOpenCamera(message);
    assert(result.ok && std::holds_alternative<cmd::CmdOpenCamera>(*result.command));

    message = messageWithId();
    result = mapper.mapCloseCamera(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.role = "left";
    result = mapper.mapCloseCamera(message);
    assert(result.ok && std::holds_alternative<cmd::CmdCloseCamera>(*result.command));
}

void testMonitorService()
{
    auto service = fakeMonitorService();
    const auto monitors = service.listMonitors();
    assert(monitors.size() == 2);
    assert(monitors[1].monitor_index == 1);
    const auto existing = service.getMonitor(1);
    assert(existing && existing->width == 1920);
    assert(!service.getMonitor(99));
}

void testWindowHandler()
{
    FakeWindowBackend backend;
    service::window::WindowService service{backend};
    runtime::WindowResourceHandlerContext context{service};

    const auto open = runWindowRequest(service, [&]
                                       {
                                           return handler::window_resource::handle(
                                               context, cmd::Command{cmd::CmdOpenWindow{"projector", "Projector", 640, 480, std::nullopt, false}});
                                       });
    assert(open.handled && open.ok);
    assert(open.values.at("window_role") == "projector");
    assert(open.values.at("width") == "640");

    const auto close = runWindowRequest(service, [&]
                                        {
                                            return handler::window_resource::handle(
                                                context, cmd::Command{cmd::CmdCloseWindow{"projector"}});
                                        });
    assert(close.handled && close.ok);

    const auto failed = runWindowRequest(service, [&]
                                         {
                                             return handler::window_resource::handle(
                                                 context, cmd::Command{cmd::CmdCloseWindow{"missing"}});
                                         });
    assert(failed.handled && !failed.ok && failed.error->code == "window_not_open");

    const auto other = handler::window_resource::handle(context, cmd::Command{cmd::CmdCaptureFrame{}});
    assert(!other.handled);
}

void testHandler()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    runtime::CameraHandlerContext context{service};

    const auto open = handler::camera::handle(context, cmd::Command{cmd::CmdOpenCamera{0, ""}});
    assert(open.handled);
    const auto close = handler::camera::handle(context, cmd::Command{cmd::CmdCloseCamera{"left"}});
    assert(close.handled);
    const auto other = handler::camera::handle(context, cmd::Command{cmd::CmdCaptureFrame{}});
    assert(!other.handled);
}

void testWindowServiceValidation()
{
    FakeWindowBackend backend;
    service::window::WindowService service{backend};

    auto result = service.openWindow(service::window::WindowOpenConfig{"", "", 640, 480, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_role");

    result = service.openWindow(service::window::WindowOpenConfig{"bad role", "", 640, 480, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_role");

    result = service.openWindow(service::window::WindowOpenConfig{"projector", "", 0, 480, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_size");

    result = service.openWindow(service::window::WindowOpenConfig{"projector", "", 640, -1, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_size");

    result = service.openWindow(service::window::WindowOpenConfig{"projector", "", 640, 480, -1, false});
    assert(!result.ok && result.error->code == "invalid_monitor_index");

    result = runWindowRequest(service, [&]
                              { return service.closeWindow("projector"); });
    assert(!result.ok && result.error->code == "window_not_open");

    result = runWindowRequest(service, [&]
                              {
                                  return service.openWindow(service::window::WindowOpenConfig{
                                      "projector", "", 640, 480, std::nullopt, false});
                              });
    assert(result.ok);
    assert(backend.last_title == "projector");
    assert(service.resolveWindowId("projector") == result.window_id);

    const auto duplicate = runWindowRequest(service, [&]
                                            {
                                                return service.openWindow(service::window::WindowOpenConfig{
                                                    "projector", "", 640, 480, std::nullopt, false});
                                            });
    assert(!duplicate.ok && duplicate.error->code == "window_already_open");

    const auto duplicate_title = runWindowRequest(service, [&]
                                                 {
                                                     return service.openWindow(service::window::WindowOpenConfig{
                                                         "projector2", "projector", 640, 480, std::nullopt, false});
                                                 });
    assert(!duplicate_title.ok && duplicate_title.error->code == "window_already_open");

    const auto close = runWindowRequest(service, [&]
                                        { return service.closeWindow("projector"); });
    assert(close.ok);
    assert(!service.resolveWindowId("projector"));

    const auto reopen_same_title = runWindowRequest(service, [&]
                                                    {
                                                        return service.openWindow(service::window::WindowOpenConfig{
                                                            "projector2", "projector", 640, 480, std::nullopt, false});
                                                    });
    assert(reopen_same_title.ok);
    assert(runWindowRequest(service, [&]
                            { return service.closeWindow("projector2"); }).ok);

    assert(runWindowRequest(service, [&]
                            {
                                return service.openWindow(service::window::WindowOpenConfig{
                                    "one", "", 100, 100, std::nullopt, false});
                            }).ok);
    assert(runWindowRequest(service, [&]
                            {
                                return service.openWindow(service::window::WindowOpenConfig{
                                    "two", "", 100, 100, std::nullopt, false});
                            }).ok);
    runWindowRequest(service, [&]
                     {
                         service.closeAll();
                         return service::window::WindowResult::success({}, win::kInvalidWindowId, 0, 0);
                     });
    assert(!service.resolveWindowId("one"));
    assert(!service.resolveWindowId("two"));
}

void testProjectorServiceValidation()
{
    FakeWindowBackend backend;
    service::window::WindowService window_service{backend};
    auto monitor_service = fakeMonitorService();
    service::projector::ProjectorService projector_service{window_service, monitor_service};

    auto result = runWindowRequest(window_service, [&]
                                   {
                                       return projector_service.openProjector(service::projector::ProjectorOpenConfig{
                                           "projector", "missing", 16, 12});
                                   });
    assert(!result.ok && result.error->code == "projector_window_not_open");

    assert(runWindowRequest(window_service, [&]
                            {
                                return window_service.openWindow(service::window::WindowOpenConfig{
                                    "projector_window", "Projector", 16, 12, std::nullopt, false});
                            }).ok);

    result = runWindowRequest(window_service, [&]
                              {
                                  return projector_service.openProjector(service::projector::ProjectorOpenConfig{
                                      "projector", "projector_window", 16, 12});
                              });
    assert(result.ok);

    const auto duplicate = runWindowRequest(window_service, [&]
                                            {
                                                return projector_service.openProjector(service::projector::ProjectorOpenConfig{
                                                    "projector", "projector_window", 16, 12});
                                            });
    assert(!duplicate.ok && duplicate.error->code == "projector_already_open");

    result = projector_service.showPattern("projector", 0);
    assert(!result.ok && result.error->code == "pattern_not_generated");

    result = projector_service.generatePatterns("projector");
    assert(result.ok && result.pattern_count > 0);

    result = projector_service.showPattern("projector", result.pattern_count);
    assert(!result.ok && result.error->code == "pattern_index_out_of_range");

    result = runWindowRequest(window_service, [&]
                              { return projector_service.showPattern("projector", 0); });
    assert(result.ok && result.pattern_index == 0);
    assert(backend.show_count == 1);
    assert(backend.last_shown_size.width == result.surface_width);
    assert(backend.last_shown_size.height == result.surface_height);

    result = runWindowRequest(window_service, [&]
                              { return projector_service.nextPattern("projector"); });
    assert(result.ok && result.pattern_index == 1);

    result = runWindowRequest(window_service, [&]
                              { return projector_service.prevPattern("projector"); });
    assert(result.ok && result.pattern_index == 0);

    const auto close_count_before = backend.close_count;
    (void)close_count_before;
    result = projector_service.closeProjector("projector");
    assert(result.ok);
    assert(backend.close_count == close_count_before);

    result = projector_service.showPattern("projector", 0);
    assert(!result.ok && result.error->code == "projector_not_open");
}

void testProjectorSurfaceConfiguration()
{
    FakeWindowBackend backend;
    service::window::WindowService window_service{backend};
    auto monitor_service = fakeMonitorService();
    service::projector::ProjectorService projector_service{window_service, monitor_service};

    auto result = projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
        "projector", 0, 1280, 720, std::nullopt, std::nullopt, service::projector::ProjectorPlacement::center});
    assert(!result.ok && result.error->code == "projector_not_open");

    assert(runWindowRequest(window_service, [&]
                            {
                                return window_service.openWindow(service::window::WindowOpenConfig{
                                    "projector_window", "Projector", 16, 12, std::nullopt, false});
                            }).ok);
    assert(runWindowRequest(window_service, [&]
                            {
                                return projector_service.openProjector(service::projector::ProjectorOpenConfig{
                                    "projector", "projector_window", 16, 12});
                            }).ok);

    result = runWindowRequest(window_service, [&]
                              {
                                  return projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
                                      "projector", 1, 1280, 720, std::nullopt, std::nullopt,
                                      service::projector::ProjectorPlacement::center});
                              });
    assert(result.ok);
    assert(result.pattern_width == 1280);
    assert(result.pattern_height == 720);
    assert(result.pattern_x == 320);
    assert(result.pattern_y == 180);
    assert(!result.clamped);
    assert(result.surface_width == 1920 && result.surface_height == 1080);
    assert(backend.configure_count == 1);

    result = runWindowRequest(window_service, [&]
                              {
                                  return projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
                                      "projector", 1, 3840, 2160, std::nullopt, std::nullopt,
                                      service::projector::ProjectorPlacement::center});
                              });
    assert(result.ok && result.pattern_width == 1920 && result.pattern_height == 1080);
    assert(result.pattern_x == 0 && result.pattern_y == 0 && result.clamped);

    result = runWindowRequest(window_service, [&]
                              {
                                  return projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
                                      "projector", 1, 640, 480, 100, 50,
                                      service::projector::ProjectorPlacement::custom});
                              });
    assert(result.ok && result.pattern_x == 100 && result.pattern_y == 50);
    assert(result.pattern_width == 640 && result.pattern_height == 480 && !result.clamped);

    result = runWindowRequest(window_service, [&]
                              {
                                  return projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
                                      "projector", 1, 1280, 480, 1000, 50,
                                      service::projector::ProjectorPlacement::custom});
                              });
    assert(result.ok && result.pattern_width == 920 && result.clamped);

    result = runWindowRequest(window_service, [&]
                              {
                                  return projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
                                      "projector", 1, 640, 480, -10, -20,
                                      service::projector::ProjectorPlacement::custom});
                              });
    assert(result.ok && result.pattern_x == 0 && result.pattern_y == 0 && result.clamped);

    result = projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
        "projector", 99, 640, 480, std::nullopt, std::nullopt, service::projector::ProjectorPlacement::center});
    assert(!result.ok && result.error->code == "monitor_not_found");

    const auto close_window = runWindowRequest(window_service, [&]
                                               { return window_service.closeWindow("projector_window"); });
    assert(close_window.ok);
    result = projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
        "projector", 1, 640, 480, std::nullopt, std::nullopt, service::projector::ProjectorPlacement::center});
    assert(!result.ok && result.error->code == "projector_window_not_open");
}

void testProjectorHandler()
{
    FakeWindowBackend backend;
    service::window::WindowService window_service{backend};
    auto monitor_service = fakeMonitorService();
    service::projector::ProjectorService projector_service{window_service, monitor_service};
    runtime::ProjectorHandlerContext context{projector_service};

    assert(runWindowRequest(window_service, [&]
                            {
                                return window_service.openWindow(service::window::WindowOpenConfig{
                                    "projector_window", "Projector", 16, 12, std::nullopt, false});
                            }).ok);

    auto result = runWindowRequest(window_service, [&]
                                   {
                                       return handler::projector::handle(
                                           context, cmd::Command{cmd::CmdOpenProjector{"projector", "projector_window", 16, 12}});
                                   });
    assert(result.handled && result.ok);

    result = handler::projector::handle(context, cmd::Command{cmd::CmdGeneratePatterns{"projector"}});
    assert(result.handled && result.ok);

    result = runWindowRequest(window_service, [&]
                              {
                                  return handler::projector::handle(
                                      context, cmd::Command{cmd::CmdProjectorShowPattern{"projector", 0}});
                              });
    assert(result.handled && result.ok);

    result = runWindowRequest(window_service, [&]
                              {
                                  return handler::projector::handle(
                                      context, cmd::Command{cmd::CmdProjectorNextPattern{"projector"}});
                              });
    assert(result.handled && result.ok);

    result = handler::projector::handle(context, cmd::Command{cmd::CmdCloseProjector{"projector"}});
    assert(result.handled && result.ok);

    const auto other = handler::projector::handle(context, cmd::Command{cmd::CmdCaptureFrame{}});
    assert(!other.handled);
}

void testServiceValidation()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    assert(!service.resolveCameraId("left"));
    const auto invalid = service.openCamera(0, "");
    assert(!invalid.ok && invalid.error->code == "invalid_command");
    const auto close = service.closeCamera("left");
    assert(!close.ok && close.error->code == "camera_not_open");
}

} // namespace

int main()
{
    testMapper();
    testWindowMapper();
    testProjectorMapper();
    testMonitorService();
    testHandler();
    testWindowHandler();
    testProjectorHandler();
    testProjectorSurfaceConfiguration();
    testServiceValidation();
    testWindowServiceValidation();
    testProjectorServiceValidation();
    return 0;
}
