#include "cmd/commands.hpp"
#include "control/control_message.hpp"
#include "handler/camera_command_handler.hpp"
#include "handler/decode_command_handler.hpp"
#include "handler/projector_command_handler.hpp"
#include "handler/scan_dataset_command_handler.hpp"
#include "handler/window_resource_command_handler.hpp"
#include "headless/headless_command_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/camera_service.hpp"
#include "service/decode_service.hpp"
#include "service/monitor_service.hpp"
#include "service/calibration_file.hpp"
#include "service/projector_service.hpp"
#include "service/scan_dataset_validator.hpp"
#include "service/scan_dataset_resolver.hpp"
#include "service/window_service.hpp"
#include "video/camera_manager.hpp"
#include "structured_light/structured_light.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <future>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <variant>

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/imgcodecs.hpp>

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


std::filesystem::path testTempDir(const std::string& name)
{
    auto path = std::filesystem::temp_directory_path() / ("tooth_scan_dataset_" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void writeScanMetadata(const std::filesystem::path& dir, int pattern_count = 2, int pattern_width = 20, int pattern_height = 16)
{
    std::ofstream output(dir / "metadata.json");
    output << "{\n"
           << "  \"scan_id\": \"session_001\",\n"
           << "  \"created_at\": \"2026-07-11T00:00:00\",\n"
           << "  \"version\": \"0.1.0\",\n"
           << "  \"projector_role\": \"projector\",\n"
           << "  \"left_role\": \"left\",\n"
           << "  \"right_role\": \"right\",\n"
           << "  \"pattern_count\": " << pattern_count << ",\n"
           << "  \"settle_ms\": 120,\n"
           << "  \"output_dir\": \"" << dir.string() << "\",\n"
           << "  \"surface\": {\n"
           << "    \"surface_width\": " << pattern_width << ",\n"
           << "    \"surface_height\": " << pattern_height << ",\n"
           << "    \"pattern_width\": " << pattern_width << ",\n"
           << "    \"pattern_height\": " << pattern_height << ",\n"
           << "    \"pattern_x\": 0,\n"
           << "    \"pattern_y\": 0\n"
           << "  }\n"
           << "}\n";
}

std::filesystem::path patternPath(const std::filesystem::path& dir, const std::string& side, int index)
{
    std::ostringstream name;
    name << "pattern_" << std::setfill('0') << std::setw(3) << index << ".png";
    return dir / side / name.str();
}

void writeImage(const std::filesystem::path& path, int width = 20, int height = 16)
{
    std::filesystem::create_directories(path.parent_path());
    cv::Mat image(height, width, CV_8UC3, cv::Scalar(10, 20, 30));
    assert(cv::imwrite(path.string(), image));
}

void writeValidScanDataset(const std::filesystem::path& dir, int pattern_count = 2)
{
    std::filesystem::create_directories(dir / "left");
    std::filesystem::create_directories(dir / "right");
    writeScanMetadata(dir, pattern_count);
    for (int index = 0; index < pattern_count; ++index)
    {
        writeImage(patternPath(dir, "left", index));
        writeImage(patternPath(dir, "right", index));
    }
}

void writeSyntheticGrayCodeDataset(const std::filesystem::path& dir, int projector_width = 8, int projector_height = 4)
{
    std::filesystem::create_directories(dir / "left");
    std::filesystem::create_directories(dir / "right");
    sl::StructuredLight structured_light{projector_width, projector_height};
    structured_light.generatePatterns();
    const auto pattern_count = static_cast<int>(structured_light.getPatternCount());
    writeScanMetadata(dir, pattern_count, projector_width, projector_height);
    for (int index = 0; index < pattern_count; ++index)
    {
        const auto& pattern = structured_light.getPattern(static_cast<std::size_t>(index));
        const bool left_written = cv::imwrite(patternPath(dir, "left", index).string(), pattern);
        const bool right_written = cv::imwrite(patternPath(dir, "right", index).string(), pattern);
        assert(left_written && right_written);
        (void)left_written;
        (void)right_written;
    }
}

cv::Mat readYmlMat(const std::filesystem::path& path, const std::string& key)
{
    cv::FileStorage storage(path.string(), cv::FileStorage::READ);
    assert(storage.isOpened());
    cv::Mat mat;
    storage[key] >> mat;
    return mat;
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

    service::monitor::MonitorService empty_service{[] { return std::vector<service::monitor::MonitorInfo>{}; }};
    assert(empty_service.listMonitors().empty());
}


void testScanMapper()
{
    video::CameraManager cameras;
    service::camera::CameraService service{cameras};
    headless::HeadlessCommandMapper mapper{service};

    auto message = messageWithId();
    message.left_role = "left";
    message.right_role = "right";
    message.output_dir = "./data/scan/test";
    auto result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.projector_role = "projector";
    message.left_role.reset();
    result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.left_role = "left";
    message.right_role.reset();
    result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.right_role = "right";
    message.output_dir.reset();
    result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.output_dir = "./data/scan/test";
    message.settle_ms = -1;
    result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.settle_ms = 0;
    message.scan_id = "session_001";
    result = mapper.mapStartScan(message);
    assert(result.ok && std::holds_alternative<cmd::CmdStartScan>(*result.command));
    const auto start = std::get<cmd::CmdStartScan>(*result.command);
    assert(start.scan_id == "session_001");
    assert(start.settle_ms == 0);

    message.scan_id = "";
    result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message = messageWithId();
    result = mapper.mapScanStatus(message);
    assert(result.ok && std::holds_alternative<cmd::CmdScanStatus>(*result.command));

    message.scan_id = "session_001";
    result = mapper.mapStopScan(message);
    assert(result.ok && std::holds_alternative<cmd::CmdStopScan>(*result.command));

    message = messageWithId();
    result = mapper.mapValidateScanDataset(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.input_dir = "";
    result = mapper.mapValidateScanDataset(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.input_dir = "./data/scan/session_001";
    result = mapper.mapValidateScanDataset(message);
    assert(result.ok && std::holds_alternative<cmd::CmdValidateScanDataset>(*result.command));
    auto validate = std::get<cmd::CmdValidateScanDataset>(*result.command);
    assert(validate.input_dir == "./data/scan/session_001");
    assert(!validate.allow_partial);

    message.allow_partial = true;
    result = mapper.mapValidateScanDataset(message);
    assert(result.ok && std::holds_alternative<cmd::CmdValidateScanDataset>(*result.command));
    validate = std::get<cmd::CmdValidateScanDataset>(*result.command);
    assert(validate.allow_partial);

    message = messageWithId();
    message.output_dir = "./data/decode/session_001";
    result = mapper.mapDecodePatterns(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.input_dir = "./data/scan/session_001";
    message.output_dir.reset();
    result = mapper.mapDecodePatterns(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.output_dir = "./data/decode/session_001";
    message.input_dir = "";
    result = mapper.mapDecodePatterns(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.input_dir = "./data/scan/session_001";
    message.output_dir = "";
    result = mapper.mapDecodePatterns(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.output_dir = "./data/decode/session_001";
    message.threshold = -1;
    result = mapper.mapDecodePatterns(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.threshold = 20;
    message.allow_partial = true;
    result = mapper.mapDecodePatterns(message);
    assert(result.ok && std::holds_alternative<cmd::CmdDecodePatterns>(*result.command));
    const auto decode = std::get<cmd::CmdDecodePatterns>(*result.command);
    assert(decode.input_dir == "./data/scan/session_001");
    assert(decode.output_dir == "./data/decode/session_001");
    assert(decode.threshold == 20);
    assert(decode.allow_partial);

    message = messageWithId();
    message.left_dir = "./captures/scan_L";
    message.right_dir = "./captures/scan_R";
    result = mapper.mapValidateScanDataset(message);
    assert(result.ok && std::holds_alternative<cmd::CmdValidateScanDataset>(*result.command));
    validate = std::get<cmd::CmdValidateScanDataset>(*result.command);
    assert(validate.input_dir.empty());
    assert(validate.left_dir == "./captures/scan_L");
    assert(validate.right_dir == "./captures/scan_R");

    message = messageWithId();
    message.left_dir = "./captures/scan_L";
    message.right_dir = "./captures/scan_R";
    message.output_dir = "./data/decode/manual_001";
    message.projector_width = 8;
    message.projector_height = 4;
    result = mapper.mapDecodePatterns(message);
    assert(result.ok && std::holds_alternative<cmd::CmdDecodePatterns>(*result.command));
    const auto explicit_decode = std::get<cmd::CmdDecodePatterns>(*result.command);
    assert(explicit_decode.input_dir.empty());
    assert(explicit_decode.left_dir == "./captures/scan_L");
    assert(explicit_decode.right_dir == "./captures/scan_R");
    assert(explicit_decode.projector_width == 8);
    assert(explicit_decode.projector_height == 4);

    message = messageWithId();
    message.image_folder = "./data/calib/mono_left";
    message.output_file = "./data/calib/mono_left.yml";
    result = mapper.mapMonoCalibrate(message);
    assert(result.ok && std::holds_alternative<cmd::CmdCalibrate>(*result.command));
    const auto mono = std::get<cmd::CmdCalibrate>(*result.command);
    assert(mono.target_camera_id == video::kInvalidCameraId);
    assert(!mono.apply_to_camera);
    assert(mono.role.empty());

    message = messageWithId();
    message.left_dir = "./data/calib/stereo_left";
    message.right_dir = "./data/calib/stereo_right";
    message.left_calibration_file = "./data/calib/mono_left.yml";
    message.right_calibration_file = "./data/calib/mono_right.yml";
    message.output_file = "./data/calib/stereo.yml";
    result = mapper.mapStereoCalibrate(message);
    assert(result.ok && std::holds_alternative<cmd::CmdStereoCalibrate>(*result.command));
    const auto stereo = std::get<cmd::CmdStereoCalibrate>(*result.command);
    assert(stereo.left_cam_id == video::kInvalidCameraId);
    assert(stereo.right_cam_id == video::kInvalidCameraId);
    assert(!stereo.apply_to_camera);
    assert(stereo.left_calibration_file == "./data/calib/mono_left.yml");
    assert(stereo.right_calibration_file == "./data/calib/mono_right.yml");
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

    service::monitor::MonitorService invalid_monitor_service{[]
                                                            {
                                                                return std::vector<service::monitor::MonitorInfo>{
                                                                    {0, 0, 0, 0, 1080, true, "invalid", false},
                                                                };
                                                            }};
    service::projector::ProjectorService invalid_projector_service{window_service, invalid_monitor_service};
    assert(invalid_projector_service.openProjector(service::projector::ProjectorOpenConfig{
               "invalid_projector", "projector_window", 16, 12})
               .ok);
    result = invalid_projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
        "invalid_projector", 0, 640, 480, std::nullopt, std::nullopt, service::projector::ProjectorPlacement::center});
    assert(!result.ok && result.error->code == "invalid_monitor_size");

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

void testScanDatasetResolver()
{
    service::scan_dataset::ScanDatasetResolver resolver;

    auto dir = testTempDir("resolver_root");
    writeValidScanDataset(dir, 2);
    service::scan_dataset::ScanDatasetInputSpec spec;
    spec.input_dir = dir;
    auto result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.root_dir == dir);
    assert(result.dataset.left_dir == dir / "left");
    assert(result.dataset.right_dir == dir / "right");
    assert(result.dataset.metadata_present);
    assert(result.dataset.pattern_count == 2);

    auto explicit_dir = testTempDir("resolver_explicit");
    std::filesystem::create_directories(explicit_dir / "scan_L");
    std::filesystem::create_directories(explicit_dir / "scan_R");
    writeImage(explicit_dir / "scan_L" / "pattern_000.png", 20, 16);
    writeImage(explicit_dir / "scan_R" / "pattern_000.png", 20, 16);
    spec = service::scan_dataset::ScanDatasetInputSpec{};
    spec.left_dir = explicit_dir / "scan_L";
    spec.right_dir = explicit_dir / "scan_R";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.left_dir == explicit_dir / "scan_L");
    assert(result.dataset.right_dir == explicit_dir / "scan_R");
    assert(result.dataset.pattern_count == 1);

    auto sibling_dir = testTempDir("resolver_sibling");
    std::filesystem::create_directories(sibling_dir / "left");
    std::filesystem::create_directories(sibling_dir / "right");
    writeImage(sibling_dir / "left" / "pattern_000.png", 20, 16);
    writeImage(sibling_dir / "right" / "pattern_000.png", 20, 16);
    spec = service::scan_dataset::ScanDatasetInputSpec{};
    spec.input_dir = sibling_dir / "left";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.root_dir == sibling_dir);
    assert(result.dataset.left_dir == sibling_dir / "left");
    assert(result.dataset.right_dir == sibling_dir / "right");

    spec = service::scan_dataset::ScanDatasetInputSpec{};
    spec.input_dir = (sibling_dir / "left").string() + "/";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.root_dir == sibling_dir);
    assert(result.dataset.left_dir == sibling_dir / "left");
    assert(result.dataset.right_dir == sibling_dir / "right");

    spec = service::scan_dataset::ScanDatasetInputSpec{};
    spec.input_dir = (sibling_dir / "right").string() + "/";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.root_dir == sibling_dir);
    assert(result.dataset.left_dir == sibling_dir / "left");
    assert(result.dataset.right_dir == sibling_dir / "right");

    spec = service::scan_dataset::ScanDatasetInputSpec{};
    spec.left_dir = (explicit_dir / "scan_L").string() + "/";
    spec.right_dir = (explicit_dir / "scan_R").string() + "/";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.left_dir == explicit_dir / "scan_L");
    assert(result.dataset.right_dir == explicit_dir / "scan_R");
}

void writeMonoCalibrationFile(const std::filesystem::path& path, const cv::Mat& K, const cv::Mat& D)
{
    cv::FileStorage fs(path.string(), cv::FileStorage::WRITE);
    fs << "RMS" << 0.25;
    fs << "K" << K;
    fs << "D" << D;
}

void assertMonoCalibrationLoadFails(const std::filesystem::path& path)
{
    std::string error;
    const auto loaded = service::calibration_file::loadMonoCalibrationFile(path, error);
    assert(!loaded);
    assert(!error.empty());
}

void testMonoCalibrationFileLoader()
{
    const auto dir = testTempDir("mono_calibration_loader");
    const auto valid_path = dir / "valid.yml";
    writeMonoCalibrationFile(valid_path, cv::Mat::eye(3, 3, CV_64F), cv::Mat::zeros(1, 5, CV_64F));
    std::string error;
    auto loaded = service::calibration_file::loadMonoCalibrationFile(valid_path, error);
    assert(loaded);
    assert(error.empty());
    assert(loaded->K.rows == 3 && loaded->K.cols == 3 && loaded->K.depth() == CV_64F);
    assert(loaded->D.total() == 5 && loaded->D.depth() == CV_64F);

    const auto valid_float_path = dir / "valid_float.yml";
    writeMonoCalibrationFile(valid_float_path, cv::Mat::eye(3, 3, CV_32F), cv::Mat::zeros(5, 1, CV_32F));
    loaded = service::calibration_file::loadMonoCalibrationFile(valid_float_path, error);
    assert(loaded);
    assert(loaded->K.depth() == CV_64F);
    assert(loaded->D.depth() == CV_64F);

    const auto invalid_k_path = dir / "invalid_k.yml";
    writeMonoCalibrationFile(invalid_k_path, cv::Mat::eye(2, 3, CV_64F), cv::Mat::zeros(1, 5, CV_64F));
    assertMonoCalibrationLoadFails(invalid_k_path);

    const auto invalid_d_shape_path = dir / "invalid_d_shape.yml";
    writeMonoCalibrationFile(invalid_d_shape_path, cv::Mat::eye(3, 3, CV_64F), cv::Mat::zeros(2, 3, CV_64F));
    assertMonoCalibrationLoadFails(invalid_d_shape_path);

    const auto invalid_d_count_path = dir / "invalid_d_count.yml";
    writeMonoCalibrationFile(invalid_d_count_path, cv::Mat::eye(3, 3, CV_64F), cv::Mat::zeros(1, 3, CV_64F));
    assertMonoCalibrationLoadFails(invalid_d_count_path);

    const auto nan_k_path = dir / "nan_k.yml";
    cv::Mat nan_k = cv::Mat::eye(3, 3, CV_64F);
    nan_k.at<double>(0, 0) = std::numeric_limits<double>::quiet_NaN();
    writeMonoCalibrationFile(nan_k_path, nan_k, cv::Mat::zeros(1, 5, CV_64F));
    assertMonoCalibrationLoadFails(nan_k_path);

    const auto inf_d_path = dir / "inf_d.yml";
    cv::Mat inf_d = cv::Mat::zeros(1, 5, CV_64F);
    inf_d.at<double>(0, 0) = std::numeric_limits<double>::infinity();
    writeMonoCalibrationFile(inf_d_path, cv::Mat::eye(3, 3, CV_64F), inf_d);
    assertMonoCalibrationLoadFails(inf_d_path);
}

void testScanDatasetValidator()
{
    service::scan_dataset::ScanDatasetValidator validator;

    auto dir = testTempDir("valid");
    writeValidScanDataset(dir, 2);
    auto result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(result.valid);
    assert(!result.partial);
    assert(result.scan_id == "session_001");
    assert(result.pattern_count == 2);
    assert(result.left_count == 2 && result.right_count == 2);
    assert(result.width == 20 && result.height == 16);
    assert(result.issues.empty());

    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir / "missing", false});
    assert(!result.valid && result.issues.size() == 1 && result.issues[0].code == "input_dir_not_found");

    dir = testTempDir("metadata_missing");
    std::filesystem::create_directories(dir / "left");
    std::filesystem::create_directories(dir / "right");
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid && result.issues[0].code == "pattern_count_not_found");

    dir = testTempDir("invalid_pattern_count");
    writeValidScanDataset(dir, 1);
    writeScanMetadata(dir, 0);
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "metadata_invalid_pattern_count"; }));

    dir = testTempDir("invalid_surface");
    writeValidScanDataset(dir, 1);
    writeScanMetadata(dir, 1, 0, 16);
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "metadata_invalid_surface"; }));

    dir = testTempDir("left_missing");
    writeValidScanDataset(dir, 1);
    std::filesystem::remove_all(dir / "left");
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "left_dir_not_found"; }));

    dir = testTempDir("right_missing");
    writeValidScanDataset(dir, 1);
    std::filesystem::remove_all(dir / "right");
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "right_dir_not_found"; }));

    dir = testTempDir("missing_left_image");
    writeValidScanDataset(dir, 2);
    std::filesystem::remove(patternPath(dir, "left", 1));
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid && result.partial && result.missing_count == 1);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "missing_left_image"; }));
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, true});
    assert(result.valid && result.partial);

    dir = testTempDir("missing_right_image");
    writeValidScanDataset(dir, 2);
    std::filesystem::remove(patternPath(dir, "right", 1));
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid && result.partial && result.missing_count == 1);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "missing_right_image"; }));

    dir = testTempDir("unreadable");
    writeValidScanDataset(dir, 1);
    std::ofstream(patternPath(dir, "left", 0)) << "not a png";
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "unreadable_left_image"; }));

    dir = testTempDir("stereo_size_mismatch");
    writeValidScanDataset(dir, 1);
    writeImage(patternPath(dir, "right", 0), 24, 16);
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "stereo_size_mismatch"; }));

    dir = testTempDir("image_size_inconsistent");
    writeValidScanDataset(dir, 2);
    writeImage(patternPath(dir, "left", 1), 22, 16);
    writeImage(patternPath(dir, "right", 1), 22, 16);
    result = validator.validate(service::scan_dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(), [](const auto& issue)
                       { return issue.code == "image_size_inconsistent"; }));
}

void testScanDatasetHandler()
{
    service::scan_dataset::ScanDatasetValidator validator;
    runtime::ScanDatasetHandlerContext context{validator};
    const auto dir = testTempDir("handler");
    writeValidScanDataset(dir, 1);

    auto result = handler::scan_dataset::handle(
        context, cmd::Command{cmd::CmdValidateScanDataset{dir.string(), false, {}, {}, {}}});
    assert(result.handled && result.ok);
    assert(result.values.at("valid") == "true");
    assert(result.values.at("issues_json") == "[]");

    const auto other = handler::scan_dataset::handle(context, cmd::Command{cmd::CmdCaptureFrame{}});
    assert(!other.handled);
}


void testDecodeServiceSyntheticDataset()
{
    service::scan_dataset::ScanDatasetValidator validator;
    service::decode::DecodeService decode_service{validator};

    const auto input_dir = testTempDir("decode_synthetic_input");
    const auto output_dir = testTempDir("decode_synthetic_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);

    auto result = decode_service.decodePatterns(service::decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
    assert(result.ok);
    assert(result.scan_id == "session_001");
    assert(result.projector_width == 8 && result.projector_height == 4);
    assert(result.image_width == 8 && result.image_height == 4);
    assert(result.left_valid_count == 32 && result.right_valid_count == 32);
    assert(result.left_valid_ratio == 1.0 && result.right_valid_ratio == 1.0);

    const auto projector_x = readYmlMat(output_dir / "left" / "projector_x.yml", "projector_x");
    const auto projector_y = readYmlMat(output_dir / "left" / "projector_y.yml", "projector_y");
    const auto mask = cv::imread((output_dir / "left" / "valid_mask.png").string(), cv::IMREAD_GRAYSCALE);
    assert(projector_x.type() == CV_32SC1);
    assert(projector_y.type() == CV_32SC1);
    assert(mask.type() == CV_8UC1);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            assert(projector_x.at<int>(y, x) == x);
            assert(projector_y.at<int>(y, x) == y);
            assert(mask.at<uchar>(y, x) == 255);
        }
    }

    const auto high_threshold_output = testTempDir("decode_high_threshold_output");
    result = decode_service.decodePatterns(service::decode::DecodePatternsConfig{input_dir, high_threshold_output, 300, false});
    assert(result.ok);
    assert(result.left_valid_count == 0 && result.right_valid_count == 0);
}

void testDecodeServiceFailures()
{
    service::scan_dataset::ScanDatasetValidator validator;
    service::decode::DecodeService decode_service{validator};

    auto input_dir = testTempDir("decode_invalid_dataset");
    auto output_dir = testTempDir("decode_invalid_output");
    auto result = decode_service.decodePatterns(service::decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
    assert(!result.ok && result.error->code == "scan_dataset_invalid");

    input_dir = testTempDir("decode_count_mismatch");
    output_dir = testTempDir("decode_count_mismatch_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);
    writeScanMetadata(input_dir, 9, 8, 4);
    result = decode_service.decodePatterns(service::decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
    assert(!result.ok && result.error->code == "decode_pattern_count_mismatch");

    input_dir = testTempDir("decode_write_failure");
    output_dir = testTempDir("decode_write_failure_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);
    std::filesystem::remove_all(output_dir);
    std::ofstream(output_dir) << "not a directory";
    result = decode_service.decodePatterns(service::decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
    assert(!result.ok && result.error->code == "decode_output_write_failed");
}

void testDecodeHandler()
{
    service::scan_dataset::ScanDatasetValidator validator;
    service::decode::DecodeService decode_service{validator};
    runtime::DecodeHandlerContext context{decode_service};
    const auto input_dir = testTempDir("decode_handler_input");
    const auto output_dir = testTempDir("decode_handler_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);

    auto result = handler::decode::handle(context, cmd::Command{cmd::CmdDecodePatterns{input_dir.string(), output_dir.string(), 15, false, {}, {}, {}, std::nullopt, std::nullopt, std::nullopt}});
    assert(result.handled && result.ok);
    assert(result.values.at("projector_width") == "8");
    assert(result.values.at("projector_height") == "4");
    assert(result.values.at("left_valid_count") == "32");

    const auto other = handler::decode::handle(context, cmd::Command{cmd::CmdCaptureFrame{}});
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
    testScanMapper();
    testMonitorService();
    testHandler();
    testWindowHandler();
    testProjectorHandler();
    testScanDatasetResolver();
    testMonoCalibrationFileLoader();
    testScanDatasetHandler();
    testDecodeHandler();
    testProjectorSurfaceConfiguration();
    testServiceValidation();
    testWindowServiceValidation();
    testProjectorServiceValidation();
    testScanDatasetValidator();
    testDecodeServiceSyntheticDataset();
    testDecodeServiceFailures();
    return 0;
}
