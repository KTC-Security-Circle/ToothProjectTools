#include "cmd/commands.hpp"
#include "control/control_message.hpp"
#include "handler/camera_command_handler.hpp"
#include "handler/window_resource_command_handler.hpp"
#include "headless/headless_command_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/camera_service.hpp"
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
        return close_result;
    }

    void pollEvents(int delay_ms) override
    {
        last_delay_ms = delay_ms;
    }

    win::WindowId next_id{1};
    bool opened{false};
    bool close_result{true};
    std::string last_title;
    int last_width{0};
    int last_height{0};
    std::optional<int> last_monitor_index;
    bool last_fullscreen{false};
    win::WindowId last_closed_id{win::kInvalidWindowId};
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
    testHandler();
    testWindowHandler();
    testServiceValidation();
    testWindowServiceValidation();
    return 0;
}
