#include "cmd/commands.hpp"
#include "capture/capture_service.hpp"
#include "control/control_message.hpp"
#include "headless/headless_command_executor.hpp"
#include "headless/headless_command_mapper.hpp"
#include "calibration/calibrator.hpp"
#include "calibration/calibration_service.hpp"
#include "calibration/stereo_calibrator.hpp"
#include "calibration/stereo_data.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "calibration/calibration_file.hpp"
#include "calibration/camera_projector_calibration_service.hpp"
#include "video/camera_service.hpp"
#include "decode/decode_service.hpp"
#include "window/monitor_service.hpp"
#include "projector/projector_service.hpp"
#include "scan/scan_event.hpp"
#include "scan/scan_service.hpp"
#include "scan/scan_dataset_resolver.hpp"
#include "scan/scan_dataset_validator.hpp"
#include "window/window_service.hpp"
#include "structured_light/structured_light.hpp"
#include "stereo_scan/stereo_scan_service.hpp"
#include "video/camera_manager.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <limits>
#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>

namespace
{
void requireCameraProjector(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class FakeWindowBackend final : public win::WindowBackend
{
  public:
    win::WindowId openWindow(const std::string& title, int width, int height, std::optional<int> monitor_index,
                             bool fullscreen) override
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

class FakeWindowActionExecutor final : public win::WindowActionExecutor
{
  public:
    win::WindowActionResult execute(win::WindowId id, const std::optional<std::string>& key,
                                    const std::optional<std::string>& action) override
    {
        called = true; window_id = id; last_key = key; last_action = action;
        return succeed ? win::WindowActionResult{true, {}, {}}
                       : win::WindowActionResult{false, "window_post_open_action_failed", "fake failure"};
    }
    bool succeed{true}; bool called{false}; win::WindowId window_id{win::kInvalidWindowId};
    std::optional<std::string> last_key; std::optional<std::string> last_action;
};

template <typename Function> auto runWindowRequest(win::WindowService& service, Function function)
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

win::MonitorService fakeMonitorService()
{
    return win::MonitorService{[]
                                            {
                                                return std::vector<win::MonitorInfo>{
                                                    {0, 0, 0, 1920, 1080, true, "primary"},
                                                    {1, 1920, 0, 1920, 1080, false, "projector"},
                                                };
                                            }};
}

win::MonitorService fakeLargeMonitorService()
{
    return win::MonitorService{[]
                                            {
                                                return std::vector<win::MonitorInfo>{
                                                    {0, 0, 0, 2240, 1400, true, "large"},
                                                };
                                            }};
}

struct CommandRuntime
{
    video::CameraManager cameras;
    video::CameraService camera_service{cameras};
    FakeWindowBackend window_backend;
    win::MonitorService monitor_service{fakeMonitorService()};
    win::WindowService window_service{window_backend, monitor_service};
    projector::ProjectorService projector_service{window_service, monitor_service};
    capture::CaptureService capture_service{cameras};
    scan::ScanEventQueue scan_events;
    scan::ScanService scan_service{projector_service, capture_service, camera_service, scan_events};
    scan::dataset::ScanDatasetValidator scan_dataset_validator;
    decode::DecodeService decode_service{scan_dataset_validator};
    calib::Calibrator calibrator;
    calib::StereoCalibrator stereo_calibrator;
    calib::StereoData stereo_data;
    reconstruction::ReconstructionService reconstruction_service;
    calib::projector::CameraProjectorCalibrationService camera_projector_calibration_service{scan_dataset_validator};
    stereo_scan::StereoScanService stereo_scan_service{
        camera_service, window_service, projector_service, scan_service, decode_service, reconstruction_service,
        scan_events, [](const std::string& role) { return common::success({{"url", role}}); },
        [](const std::string&) { return common::success(); }};
    headless::HeadlessCommandExecutor executor{
        camera_service, window_service, projector_service, scan_service, scan_dataset_validator,
        decode_service, capture_service, cameras, &calibrator, &stereo_calibrator, stereo_data,
        reconstruction_service, camera_projector_calibration_service, stereo_scan_service};
};

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

void writeScanMetadata(const std::filesystem::path& dir, int pattern_count = 2, int pattern_width = 20,
                       int pattern_height = 16)
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
           << "  \"sync\": \"photodiode\",\n"
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

void testStructuredLightPatternGeneration()
{
    constexpr int width = 32;
    constexpr int height = 24;
    const auto require = [](bool condition, const char* message) {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    };
    sl::StructuredLight structured_light{width, height};

    structured_light.generatePatterns();

    const auto pattern_count = structured_light.getPatternCount();
    require(pattern_count > 2, "StructuredLight did not generate Gray Code and reference patterns");
    for (std::size_t index = 0; index < pattern_count; ++index)
    {
        const auto& pattern = structured_light.getPattern(index);
        require(pattern.cols == width, "StructuredLight pattern width does not match logical resolution");
        require(pattern.rows == height, "StructuredLight pattern height does not match logical resolution");
    }

    const auto& white = structured_light.getPattern(pattern_count - 2);
    const auto& black = structured_light.getPattern(pattern_count - 1);
    require(cv::countNonZero(white != 255) == 0, "StructuredLight penultimate pattern is not white");
    require(cv::countNonZero(black) == 0, "StructuredLight final pattern is not black");
    require(projector::photodiodeMarkerValue(pattern_count - 2) ==
                (((pattern_count - 2) % 2 == 0) ? 0 : 255),
            "FULL WHITE marker does not follow index parity");
    require(projector::photodiodeMarkerValue(pattern_count - 1) ==
                (((pattern_count - 1) % 2 == 0) ? 0 : 255),
            "FULL BLACK marker does not follow index parity");

    bool out_of_range_thrown = false;
    try
    {
        (void)structured_light.getPattern(pattern_count);
    }
    catch (const std::out_of_range&)
    {
        out_of_range_thrown = true;
    }
    require(out_of_range_thrown, "StructuredLight out-of-range access did not throw");
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
    video::CameraService service{cameras};
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
    video::CameraService service{cameras};
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
    video::CameraService service{cameras};
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

    const auto primary = service.resolveMonitor(std::nullopt);
    assert(primary && primary->monitor.monitor_index == 0 && !primary->fallback);
    const auto valid = service.resolveMonitor(1);
    assert(valid && valid->monitor.monitor_index == 1 && !valid->fallback);
    const auto fallback = service.resolveMonitor(2);
    assert(fallback && fallback->monitor.monitor_index == 0 && fallback->fallback);

    win::MonitorService empty_service{[] { return std::vector<win::MonitorInfo>{}; }};
    assert(empty_service.listMonitors().empty());
    assert(!empty_service.resolveMonitor(std::nullopt));
    assert(!empty_service.resolveMonitor(1));
}

void testScanMapper()
{
    video::CameraManager cameras;
    video::CameraService service{cameras};
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
    assert(result.ok && std::get<cmd::CmdStartScan>(*result.command).right_role.empty());

    message.right_role = "right";
    message.output_dir.reset();
    result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "missing_field");

    message.output_dir = "./data/scan/test";
    message.sync_mode = "photodiode";
    message.photodiode_baud = 0;
    result = mapper.mapStartScan(message);
    assert(!result.ok && result.error->code == "invalid_command");

    message.photodiode_baud = 115200;
    message.photodiode_device = "/dev/ttyACM0";
    message.max_patterns = 1;
    message.scan_id = "session_001";
    result = mapper.mapStartScan(message);
    assert(result.ok && std::holds_alternative<cmd::CmdStartScan>(*result.command));
    const auto start = std::get<cmd::CmdStartScan>(*result.command);
    assert(start.scan_id == "session_001");
    assert(start.photodiode_device == "/dev/ttyACM0" && start.photodiode_baud == 115200);
    assert(start.max_patterns == 1);

    message.max_patterns = 0;
    result = mapper.mapStartScan(message);
    assert(result.ok && std::get<cmd::CmdStartScan>(*result.command).max_patterns == 0);

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
    assert(!result.ok && result.error->code == "missing_field");

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
    assert(!result.ok && result.error->code == "missing_field");

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
    message.board_corners_x = 10;
    message.board_corners_y = 7;
    message.square_size_mm = 12.5;
    result = mapper.mapMonoCalibrate(message);
    assert(result.ok && std::holds_alternative<cmd::CmdCalibrate>(*result.command));
    const auto mono = std::get<cmd::CmdCalibrate>(*result.command);
    assert(mono.target_camera_id == video::kInvalidCameraId);
    assert(!mono.apply_to_camera);
    assert(mono.role.empty());
    assert(mono.board_corners_x == 10);
    assert(mono.board_corners_y == 7);
    assert(mono.square_size_mm == 12.5);

    auto default_mono_message = messageWithId();
    default_mono_message.board_corners_x = 10; default_mono_message.board_corners_y = 7;
    default_mono_message.square_size_mm = 12.5;
    auto default_mono_result = mapper.mapMonoCalibrate(default_mono_message);
    assert(default_mono_result.ok);
    const auto default_mono = std::get<cmd::CmdCalibrate>(*default_mono_result.command);
    assert(default_mono.image_folder == "data/calib/mono_left" &&
           default_mono.output_file == "data/calib/mono_left.yml");

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

    const auto default_stereo_result = mapper.mapStereoCalibrate(messageWithId());
    assert(default_stereo_result.ok);
    const auto default_stereo = std::get<cmd::CmdStereoCalibrate>(*default_stereo_result.command);
    assert(default_stereo.left_dir == "data/calib/stereo/left" &&
           default_stereo.right_dir == "data/calib/stereo/right" &&
           default_stereo.output_file == "data/calib/stereo.yml");
}

void testStereoScanMapper()
{
    video::CameraManager cameras;
    video::CameraService service{cameras};
    headless::HeadlessCommandMapper mapper{service};
    auto message = messageWithId();
    message.monitor_index = 1;
    auto mapped = mapper.mapStereoScan(message);
    requireCameraProjector(mapped.ok, "minimal stereo_scan did not map");
    const auto defaults = std::get<cmd::CmdStereoScan>(*mapped.command);
    requireCameraProjector(defaults.left_camera_id == 0 && defaults.right_camera_id == 2,
                           "stereo camera defaults differ");
    requireCameraProjector(defaults.calibration_file == "data/calib/stereo.yml" &&
                           defaults.sync_mode == "delay" && defaults.delay_ms == 100,
                           "stereo_scan defaults differ");
    requireCameraProjector(defaults.output_dir.parent_path() == std::filesystem::path{"data/scans"} &&
                           defaults.ply_file == defaults.output_dir / "cloud.ply",
                           "generated output contract differs");

    message.left_camera_id = 4; message.right_camera_id = 5;
    message.calibration_file = "custom/stereo.yml"; message.output_dir = "custom/scan";
    message.ply_file = "custom/cloud.ply"; message.sync_mode = "photodiode";
    message.guard_ms = 77; message.decode_threshold = 21;
    mapped = mapper.mapStereoScan(message);
    requireCameraProjector(mapped.ok, "explicit stereo_scan did not map");
    const auto explicit_config = std::get<cmd::CmdStereoScan>(*mapped.command);
    requireCameraProjector(explicit_config.left_camera_id == 4 && explicit_config.right_camera_id == 5 &&
                           explicit_config.guard_ms == 77 && explicit_config.decode_threshold == 21 &&
                           explicit_config.output_dir == "custom/scan", "explicit stereo_scan values differ");

    message.guard_ms = -1;
    requireCameraProjector(!mapper.mapStereoScan(message).ok, "negative guard_ms was accepted");
    message.guard_ms = 1; message.sync_mode = "bad";
    requireCameraProjector(!mapper.mapStereoScan(message).ok, "invalid sync_mode was accepted");
}

void testWindowHandler()
{
    CommandRuntime runtime;

    const auto open = runWindowRequest(
        runtime.window_service,
        [&]
        {
            return runtime.executor.execute(
                cmd::Command{cmd::CmdOpenWindow{"projector", "Projector", 640, 480, std::nullopt, false}});
        });
    assert(open.handled && open.ok);
    assert(open.values.at("window_role") == "projector");
    assert(open.values.at("width") == "640");

    const auto close = runWindowRequest(
        runtime.window_service,
        [&] { return runtime.executor.execute(cmd::Command{cmd::CmdCloseWindow{"projector"}}); });
    assert(close.handled && close.ok);

    const auto failed = runWindowRequest(
        runtime.window_service,
        [&] { return runtime.executor.execute(cmd::Command{cmd::CmdCloseWindow{"missing"}}); });
    assert(failed.handled && !failed.ok && failed.error->code == "window_not_open");
}

void testHandler()
{
    CommandRuntime runtime;

    const auto open = runtime.executor.execute(cmd::Command{cmd::CmdOpenCamera{0, ""}});
    assert(open.handled);
    const auto close = runtime.executor.execute(cmd::Command{cmd::CmdCloseCamera{"left"}});
    assert(close.handled);
}

void testMonitorFallbackAcrossServices()
{
    FakeWindowBackend backend;
    win::MonitorService one_monitor{[] {
        return std::vector<win::MonitorInfo>{{0, 0, 0, 2240, 1400, true, "primary", false}};
    }};
    win::WindowService window_service{backend, one_monitor};

    auto result =
        window_service.openWindow(win::WindowOpenConfig{"default", "", 640, 480, std::nullopt, false});
    assert(result.ok && backend.last_monitor_index == 0);
    result = window_service.openWindow(win::WindowOpenConfig{"valid", "", 640, 480, 0, false});
    assert(result.ok && backend.last_monitor_index == 0);
    result = window_service.openWindow(win::WindowOpenConfig{"fallback", "", 640, 480, 1, false});
    assert(result.ok && backend.last_monitor_index == 0);

    FakeWindowBackend empty_backend;
    win::MonitorService no_monitors{[] { return std::vector<win::MonitorInfo>{}; }};
    win::WindowService empty_window_service{empty_backend, no_monitors};
    result = empty_window_service.openWindow(win::WindowOpenConfig{"missing", "", 640, 480, 0, false});
    assert(!result.ok && result.error->code == "monitor_not_found");
    assert(empty_backend.next_id == 1 && !empty_backend.opened);

    projector::ProjectorService empty_projector{window_service, no_monitors};
    auto projector_result =
        empty_projector.openProjector(projector::ProjectorOpenConfig{"missing", "default", 640, 480});
    assert(!projector_result.ok && projector_result.error->code == "monitor_not_found");
    assert(!empty_projector.scanSnapshot("missing"));
}

void testWindowServiceValidation()
{
    FakeWindowBackend backend;
    win::WindowService service{backend};

    auto result = service.openWindow(win::WindowOpenConfig{"", "", 640, 480, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_role");

    result = service.openWindow(win::WindowOpenConfig{"bad role", "", 640, 480, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_role");

    result = service.openWindow(win::WindowOpenConfig{"projector", "", 0, 480, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_size");

    result = service.openWindow(win::WindowOpenConfig{"projector", "", 640, -1, std::nullopt, false});
    assert(!result.ok && result.error->code == "invalid_window_size");

    result = service.openWindow(win::WindowOpenConfig{"projector", "", 640, 480, -1, false});
    assert(!result.ok && result.error->code == "invalid_monitor_index");

    result = runWindowRequest(service, [&] { return service.closeWindow("projector"); });
    assert(!result.ok && result.error->code == "window_not_open");

    result = runWindowRequest(service,
                              [&] {
                                  return service.openWindow(win::WindowOpenConfig{"projector", "", 640, 480,
                                                                                              std::nullopt, false});
                              });
    assert(result.ok);
    assert(backend.last_title == "projector");
    assert(service.resolveWindowId("projector") == result.window_id);

    const auto duplicate = runWindowRequest(service,
                                            [&] {
                                                return service.openWindow(win::WindowOpenConfig{
                                                    "projector", "", 640, 480, std::nullopt, false});
                                            });
    assert(!duplicate.ok && duplicate.error->code == "window_already_open");

    const auto duplicate_title = runWindowRequest(service,
                                                  [&]
                                                  {
                                                      return service.openWindow(win::WindowOpenConfig{
                                                          "projector2", "projector", 640, 480, std::nullopt, false});
                                                  });
    assert(!duplicate_title.ok && duplicate_title.error->code == "window_already_open");

    const auto close = runWindowRequest(service, [&] { return service.closeWindow("projector"); });
    assert(close.ok);
    assert(!service.resolveWindowId("projector"));

    const auto reopen_same_title = runWindowRequest(service,
                                                    [&]
                                                    {
                                                        return service.openWindow(win::WindowOpenConfig{
                                                            "projector2", "projector", 640, 480, std::nullopt, false});
                                                    });
    assert(reopen_same_title.ok);
    assert(runWindowRequest(service, [&] { return service.closeWindow("projector2"); }).ok);

    assert(
        runWindowRequest(
            service,
            [&] {
                return service.openWindow(win::WindowOpenConfig{"one", "", 100, 100, std::nullopt, false});
            })
            .ok);
    assert(
        runWindowRequest(
            service,
            [&] {
                return service.openWindow(win::WindowOpenConfig{"two", "", 100, 100, std::nullopt, false});
            })
            .ok);
    runWindowRequest(service,
                     [&]
                     {
                         service.closeAll();
                         return win::WindowResult::success({}, win::kInvalidWindowId, 0, 0);
                     });
    assert(!service.resolveWindowId("one"));
    assert(!service.resolveWindowId("two"));
}

void testWindowPostOpenAction()
{
    FakeWindowBackend backend;
    win::WindowService service{backend};
    auto unsupported = service.openWindow({"unsupported", "", 640, 480, std::nullopt, false,
                                           "Super+Shift+Right", std::nullopt});
    requireCameraProjector(!unsupported.ok && unsupported.error->code == "window_post_open_action_unsupported",
                           "unsupported post-open action was ignored");
    FakeWindowActionExecutor actions;
    service.setWindowActionExecutor(&actions);
    auto success = service.openWindow({"success", "", 640, 480, std::nullopt, false,
                                       "Super+Shift+Right", std::nullopt});
    requireCameraProjector(success.ok && actions.called && actions.last_key == "Super+Shift+Right",
                           "post-open key was not executed");
    actions.succeed = false; actions.called = false;
    auto failed = service.openWindow({"failed", "", 640, 480, std::nullopt, false,
                                      std::nullopt, "move-to-output-right"});
    requireCameraProjector(!failed.ok && actions.called && failed.error->code == "window_post_open_action_failed",
                           "post-open action failure was not propagated");
}

void testProjectorServiceValidation()
{
    FakeWindowBackend backend;
    win::WindowService window_service{backend};
    auto monitor_service = fakeMonitorService();
    projector::ProjectorService projector_service{window_service, monitor_service};

    auto result = runWindowRequest(window_service,
                                   [&] {
                                       return projector_service.openProjector(
                                           projector::ProjectorOpenConfig{"projector", "missing", 16, 12});
                                   });
    assert(!result.ok && result.error->code == "projector_window_not_open");

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 16, 12, std::nullopt, false});
                            })
               .ok);

    result = runWindowRequest(window_service,
                              [&]
                              {
                                  return projector_service.openProjector(
                                      projector::ProjectorOpenConfig{"projector", "projector_window", 16, 12});
                              });
    assert(result.ok);

    const auto duplicate =
        runWindowRequest(window_service,
                         [&]
                         {
                             return projector_service.openProjector(
                                 projector::ProjectorOpenConfig{"projector", "projector_window", 16, 12});
                         });
    assert(!duplicate.ok && duplicate.error->code == "projector_already_open");

    result = projector_service.showPattern("projector", 0);
    assert(!result.ok && result.error->code == "pattern_not_generated");

    result = projector_service.generatePatterns("projector");
    assert(result.ok && result.pattern_count > 0);

    result = projector_service.showPattern("projector", result.pattern_count);
    assert(!result.ok && result.error->code == "pattern_index_out_of_range");

    result = runWindowRequest(window_service, [&] { return projector_service.showPattern("projector", 0); });
    assert(result.ok && result.pattern_index == 0);
    assert(backend.show_count == 1);
    assert(backend.last_shown_size.width == result.surface_width);
    assert(backend.last_shown_size.height == result.surface_height);

    result = runWindowRequest(window_service, [&] { return projector_service.nextPattern("projector"); });
    assert(result.ok && result.pattern_index == 1);

    result = runWindowRequest(window_service, [&] { return projector_service.prevPattern("projector"); });
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
    win::WindowService window_service{backend};
    auto monitor_service = fakeMonitorService();
    projector::ProjectorService projector_service{window_service, monitor_service};

    auto result = projector_service.configureSurface(projector::ProjectorSurfaceRequest{
        "projector", 0, 1280, 720, std::nullopt, std::nullopt, projector::ProjectorPlacement::center});
    assert(!result.ok && result.error->code == "projector_not_open");

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 16, 12, std::nullopt, false});
                            })
               .ok);
    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return projector_service.openProjector(
                                    projector::ProjectorOpenConfig{"projector", "projector_window", 16, 12});
                            })
               .ok);

    result = runWindowRequest(
        window_service,
        [&]
        {
            return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                "projector", 1, 1280, 720, std::nullopt, std::nullopt, projector::ProjectorPlacement::center});
        });
    assert(result.ok);
    assert(result.pattern_width == 1280);
    assert(result.pattern_height == 720);
    assert(result.pattern_x == 320);
    assert(result.pattern_y == 180);
    assert(!result.clamped);
    assert(result.surface_width == 1920 && result.surface_height == 1080);
    assert(backend.configure_count == 1);

    result = runWindowRequest(window_service,
                              [&]
                              {
                                  return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                      "projector", 1, 3840, 2160, std::nullopt, std::nullopt,
                                      projector::ProjectorPlacement::center});
                              });
    assert(result.ok && result.pattern_width == 1920 && result.pattern_height == 1080);
    assert(result.pattern_x == 0 && result.pattern_y == 0 && result.clamped);

    result =
        runWindowRequest(window_service,
                         [&]
                         {
                             return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                 "projector", 1, 640, 480, 100, 50, projector::ProjectorPlacement::custom});
                         });
    assert(result.ok && result.pattern_x == 100 && result.pattern_y == 50);
    assert(result.pattern_width == 640 && result.pattern_height == 480 && !result.clamped);

    result =
        runWindowRequest(window_service,
                         [&]
                         {
                             return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                 "projector", 1, 1280, 480, 1000, 50, projector::ProjectorPlacement::custom});
                         });
    assert(result.ok && result.pattern_width == 920 && result.clamped);

    result =
        runWindowRequest(window_service,
                         [&]
                         {
                             return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                 "projector", 1, 640, 480, -10, -20, projector::ProjectorPlacement::custom});
                         });
    assert(result.ok && result.pattern_x == 0 && result.pattern_y == 0 && result.clamped);

    result = projector_service.configureSurface(projector::ProjectorSurfaceRequest{
        "projector", 99, 640, 480, std::nullopt, std::nullopt, projector::ProjectorPlacement::center});
    assert(result.ok && result.monitor_index == 0);
    assert(backend.last_configured_monitor_index == 0);

    win::MonitorService invalid_monitor_service{[]
                                                             {
                                                                 return std::vector<win::MonitorInfo>{
                                                                     {0, 0, 0, 0, 1080, true, "invalid", false},
                                                                 };
                                                             }};
    projector::ProjectorService invalid_projector_service{window_service, invalid_monitor_service};
    assert(invalid_projector_service
               .openProjector(projector::ProjectorOpenConfig{"invalid_projector", "projector_window", 16, 12})
               .ok);
    result = invalid_projector_service.configureSurface(projector::ProjectorSurfaceRequest{
        "invalid_projector", 0, 640, 480, std::nullopt, std::nullopt, projector::ProjectorPlacement::center});
    assert(!result.ok && result.error->code == "invalid_monitor_size");

    const auto close_window =
        runWindowRequest(window_service, [&] { return window_service.closeWindow("projector_window"); });
    assert(close_window.ok);
    result = projector_service.configureSurface(projector::ProjectorSurfaceRequest{
        "projector", 1, 640, 480, std::nullopt, std::nullopt, projector::ProjectorPlacement::center});
    assert(!result.ok && result.error->code == "projector_window_not_open");
}

void assertOnlyBinaryValues(const cv::Mat& image)
{
    assert(!image.empty());
    std::set<int> values;
    for (int y = 0; y < image.rows; ++y)
    {
        for (int x = 0; x < image.cols; ++x)
        {
            if (image.channels() == 1)
            {
                values.insert(image.at<uchar>(y, x));
            }
            else
            {
                const auto pixel = image.at<cv::Vec3b>(y, x);
                values.insert(pixel[0]);
                values.insert(pixel[1]);
                values.insert(pixel[2]);
            }
        }
    }
    assert(values.count(0) == 1);
    assert(values.count(255) == 1);
    assert(values.size() == 2);
}

void testProjectorLogicalResolutionSeparateFromDisplay()
{
    FakeWindowBackend backend;
    win::WindowService window_service{backend};
    auto monitor_service = fakeLargeMonitorService();
    projector::ProjectorService projector_service{window_service, monitor_service};

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 960, 540, std::nullopt, false});
                            })
               .ok);
    auto result = runWindowRequest(window_service,
                                   [&]
                                   {
                                       return projector_service.openProjector(projector::ProjectorOpenConfig{
                                           "projector", "projector_window", 960, 540});
                                   });
    assert(result.ok);
    assert(result.width == 960 && result.height == 540);
    assert(result.code_width == 960 && result.code_height == 540);

    result = runWindowRequest(window_service,
                              [&]
                              {
                                  return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                      "projector", 0, 1920, 1080, std::nullopt, std::nullopt,
                                      projector::ProjectorPlacement::center});
                              });
    assert(result.ok);
    assert(result.code_width == 960 && result.code_height == 540);
    assert(result.surface_width == 2240 && result.surface_height == 1400);
    assert(result.pattern_width == 1920 && result.pattern_height == 1080);
    assert(result.display_width == 1920 && result.display_height == 1080);
    assert(result.pattern_x == 160 && result.pattern_y == 160);
    assert(result.display_x == 160 && result.display_y == 160);

    result = projector_service.generatePatterns("projector");
    assert(result.ok);
    assert(result.width == 960 && result.height == 540);
    assert(result.code_width == 960 && result.code_height == 540);
    assert(result.pattern_width == 1920 && result.pattern_height == 1080);
    assert(result.display_width == 1920 && result.display_height == 1080);
    assert(result.pattern_count == 42);

    result = runWindowRequest(window_service, [&] { return projector_service.showPattern("projector", 0); });
    assert(result.ok);
    assert(backend.last_shown_size.width == 2240 && backend.last_shown_size.height == 1400);
}

void testProjectorSameResolutionStillGeneratesExpectedCount()
{
    FakeWindowBackend backend;
    win::WindowService window_service{backend};
    auto monitor_service = fakeLargeMonitorService();
    projector::ProjectorService projector_service{window_service, monitor_service};

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 1920, 1080, std::nullopt, false});
                            })
               .ok);
    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return projector_service.openProjector(projector::ProjectorOpenConfig{
                                    "projector", "projector_window", 1920, 1080});
                            })
               .ok);
    auto result =
        runWindowRequest(window_service,
                         [&]
                         {
                             return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                 "projector", 0, 1920, 1080, std::nullopt, std::nullopt,
                                 projector::ProjectorPlacement::center});
                         });
    assert(result.ok);
    result = projector_service.generatePatterns("projector");
    assert(result.ok);
    assert(result.width == 1920 && result.height == 1080);
    assert(result.display_width == 1920 && result.display_height == 1080);
    assert(result.pattern_count == 46);
}

void testProjectorSurfaceReconfigurationKeepsGeneratedPatterns()
{
    FakeWindowBackend backend;
    win::WindowService window_service{backend};
    auto monitor_service = fakeLargeMonitorService();
    projector::ProjectorService projector_service{window_service, monitor_service};

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 960, 540, std::nullopt, false});
                            })
               .ok);
    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return projector_service.openProjector(
                                    projector::ProjectorOpenConfig{"projector", "projector_window", 960, 540});
                            })
               .ok);
    auto result =
        runWindowRequest(window_service,
                         [&]
                         {
                             return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                 "projector", 0, 1920, 1080, std::nullopt, std::nullopt,
                                 projector::ProjectorPlacement::center});
                         });
    assert(result.ok);
    result = projector_service.generatePatterns("projector");
    assert(result.ok && result.pattern_count == 42);

    result = runWindowRequest(
        window_service,
        [&]
        {
            return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                "projector", 0, 1280, 720, std::nullopt, std::nullopt, projector::ProjectorPlacement::center});
        });
    assert(result.ok);
    assert(result.code_width == 960 && result.code_height == 540);
    assert(result.pattern_count == 42);
    assert(result.display_width == 1280 && result.display_height == 720);

    result = runWindowRequest(window_service, [&] { return projector_service.showPattern("projector", 0); });
    assert(result.ok);
    assert(backend.last_shown_size.width == 2240 && backend.last_shown_size.height == 1400);
}

void testProjectorSurfaceChangesBeforeGenerateKeepCodeResolution()
{
    FakeWindowBackend backend;
    win::WindowService window_service{backend};
    auto monitor_service = fakeLargeMonitorService();
    projector::ProjectorService projector_service{window_service, monitor_service};

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 960, 540, std::nullopt, false});
                            })
               .ok);
    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return projector_service.openProjector(
                                    projector::ProjectorOpenConfig{"projector", "projector_window", 960, 540});
                            })
               .ok);
    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                    "projector", 0, 1920, 1080, std::nullopt, std::nullopt,
                                    projector::ProjectorPlacement::center});
                            })
               .ok);
    auto result = runWindowRequest(
        window_service,
        [&]
        {
            return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                "projector", 0, 1280, 720, std::nullopt, std::nullopt, projector::ProjectorPlacement::center});
        });
    assert(result.ok);
    assert(result.code_width == 960 && result.code_height == 540);
    assert(result.display_width == 1280 && result.display_height == 720);

    result = projector_service.generatePatterns("projector");
    assert(result.ok);
    assert(result.width == 960 && result.height == 540);
    assert(result.pattern_count == 42);
    assert(result.display_width == 1280 && result.display_height == 720);
}

void testProjectorNearestResizeProducesBinaryValues()
{
    FakeWindowBackend backend;
    win::WindowService window_service{backend};
    auto monitor_service = fakeLargeMonitorService();
    projector::ProjectorService projector_service{window_service, monitor_service};

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 4, 4, std::nullopt, false});
                            })
               .ok);
    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return projector_service.openProjector(
                                    projector::ProjectorOpenConfig{"projector", "projector_window", 4, 4});
                            })
               .ok);
    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return projector_service.configureSurface(projector::ProjectorSurfaceRequest{
                                    "projector", 0, 8, 8, 0, 0, projector::ProjectorPlacement::custom});
                            })
               .ok);
    auto result = projector_service.generatePatterns("projector");
    assert(result.ok);
    result = runWindowRequest(window_service, [&] { return projector_service.showPattern("projector", 0); });
    assert(result.ok);
    assertOnlyBinaryValues(backend.last_image);
}

void testProjectorHandlerReportsCodeAndDisplayResolution()
{
    CommandRuntime runtime;
    auto& window_service = runtime.window_service;

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 960, 540, std::nullopt, false});
                            })
               .ok);
    auto result = runWindowRequest(
        window_service,
        [&]
        {
            return runtime.executor.execute(cmd::Command{cmd::CmdOpenProjector{"projector", "projector_window", 960, 540}});
        });
    assert(result.handled && result.ok);
    result = runWindowRequest(
        window_service,
        [&]
        {
            return runtime.executor.execute(cmd::Command{cmd::CmdConfigureProjectorSurface{"projector", 0, 1920, 1080, {}, {}, "center"}});
        });
    assert(result.handled && result.ok);
    assert(result.values.at("code_width") == "960");
    assert(result.values.at("code_height") == "540");
    assert(result.values.at("display_width") == "1920");
    assert(result.values.at("display_height") == "1080");
    assert(result.values.at("display_x") == "160");
    assert(result.values.at("display_y") == "160");

    result = runtime.executor.execute(cmd::Command{cmd::CmdGeneratePatterns{"projector"}});
    assert(result.handled && result.ok);
    assert(result.values.at("width") == "960");
    assert(result.values.at("height") == "540");
    assert(result.values.at("code_width") == "960");
    assert(result.values.at("code_height") == "540");
    assert(result.values.at("display_width") == "1920");
    assert(result.values.at("display_height") == "1080");
    assert(result.values.at("pattern_count") == "42");
}

void testScanMetadataDistinguishesCodeAndDisplayResolution()
{
    const auto dir = testTempDir("metadata_code_display");
    std::ofstream output(dir / "metadata.json");
    output << "{\n"
           << "  \"scan_id\": \"session_001\",\n"
           << "  \"pattern_count\": 42,\n"
           << "  \"projector_width\": 960,\n"
           << "  \"projector_height\": 540,\n"
           << "  \"surface\": {\n"
           << "    \"surface_width\": 2240,\n"
           << "    \"surface_height\": 1400,\n"
           << "    \"display_width\": 1920,\n"
           << "    \"display_height\": 1080,\n"
           << "    \"display_x\": 160,\n"
           << "    \"display_y\": 160,\n"
           << "    \"pattern_width\": 1920,\n"
           << "    \"pattern_height\": 1080,\n"
           << "    \"pattern_x\": 160,\n"
           << "    \"pattern_y\": 160\n"
           << "  }\n"
           << "}\n";
    output.close();

    scan::dataset::ScanDatasetValidator validator;
    std::vector<scan::dataset::ScanDatasetIssue> issues;
    const auto metadata = validator.readMetadataFileForDecode(dir / "metadata.json", issues);
    assert(metadata);
    assert(issues.empty());
    assert(metadata->projector_width == 960 && metadata->projector_height == 540);
    assert(metadata->pattern_count == 42);
    assert(metadata->surface_width == 2240 && metadata->surface_height == 1400);
    assert(metadata->display_width == 1920 && metadata->display_height == 1080);
    assert(metadata->display_x == 160 && metadata->display_y == 160);
}

void testProjectorHandler()
{
    CommandRuntime runtime;
    auto& window_service = runtime.window_service;

    assert(runWindowRequest(window_service,
                            [&]
                            {
                                return window_service.openWindow(win::WindowOpenConfig{
                                    "projector_window", "Projector", 16, 12, std::nullopt, false});
                            })
               .ok);

    auto result =
        runWindowRequest(window_service,
                         [&]
                         {
                             return runtime.executor.execute(cmd::Command{cmd::CmdOpenProjector{"projector", "projector_window", 16, 12}});
                         });
    assert(result.handled && result.ok);

    result = runtime.executor.execute(cmd::Command{cmd::CmdGeneratePatterns{"projector"}});
    assert(result.handled && result.ok);

    result = runWindowRequest(
        window_service,
        [&] {
            return runtime.executor.execute(cmd::Command{cmd::CmdProjectorShowPattern{"projector", 0}});
        });
    assert(result.handled && result.ok);

    result = runWindowRequest(
        window_service,
        [&] { return runtime.executor.execute(cmd::Command{cmd::CmdProjectorNextPattern{"projector"}}); });
    assert(result.handled && result.ok);

    result = runtime.executor.execute(cmd::Command{cmd::CmdCloseProjector{"projector"}});
    assert(result.handled && result.ok);

}

void testScanDatasetResolver()
{
    scan::dataset::ScanDatasetResolver resolver;

    auto dir = testTempDir("resolver_root");
    writeValidScanDataset(dir, 2);
    scan::dataset::ScanDatasetInputSpec spec;
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
    spec = scan::dataset::ScanDatasetInputSpec{};
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
    spec = scan::dataset::ScanDatasetInputSpec{};
    spec.input_dir = sibling_dir / "left";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.root_dir == sibling_dir);
    assert(result.dataset.left_dir == sibling_dir / "left");
    assert(result.dataset.right_dir == sibling_dir / "right");

    spec = scan::dataset::ScanDatasetInputSpec{};
    spec.input_dir = (sibling_dir / "left").string() + "/";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.root_dir == sibling_dir);
    assert(result.dataset.left_dir == sibling_dir / "left");
    assert(result.dataset.right_dir == sibling_dir / "right");

    spec = scan::dataset::ScanDatasetInputSpec{};
    spec.input_dir = (sibling_dir / "right").string() + "/";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.root_dir == sibling_dir);
    assert(result.dataset.left_dir == sibling_dir / "left");
    assert(result.dataset.right_dir == sibling_dir / "right");

    spec = scan::dataset::ScanDatasetInputSpec{};
    spec.left_dir = (explicit_dir / "scan_L").string() + "/";
    spec.right_dir = (explicit_dir / "scan_R").string() + "/";
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.left_dir == explicit_dir / "scan_L");
    assert(result.dataset.right_dir == explicit_dir / "scan_R");

    auto count_override_dir = testTempDir("resolver_count_override");
    std::filesystem::create_directories(count_override_dir / "left");
    std::filesystem::create_directories(count_override_dir / "right");
    writeImage(count_override_dir / "left" / "pattern_099.png", 20, 16);
    writeImage(count_override_dir / "right" / "pattern_099.png", 20, 16);
    spec = scan::dataset::ScanDatasetInputSpec{};
    spec.input_dir = count_override_dir;
    spec.pattern_count = 7;
    result = resolver.resolve(spec);
    assert(result.ok);
    assert(result.dataset.pattern_count == 7);
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
    const auto loaded = calib::file::loadMonoCalibrationFile(path, error);
    assert(!loaded);
    assert(!error.empty());
}

void testMonoCalibrationFileLoader()
{
    const auto dir = testTempDir("mono_calibration_loader");
    const auto valid_path = dir / "valid.yml";
    writeMonoCalibrationFile(valid_path, cv::Mat::eye(3, 3, CV_64F), cv::Mat::zeros(1, 5, CV_64F));
    std::string error;
    auto loaded = calib::file::loadMonoCalibrationFile(valid_path, error);
    assert(loaded);
    assert(error.empty());
    assert(loaded->K.rows == 3 && loaded->K.cols == 3 && loaded->K.depth() == CV_64F);
    assert(loaded->D.total() == 5 && loaded->D.depth() == CV_64F);

    const auto valid_float_path = dir / "valid_float.yml";
    writeMonoCalibrationFile(valid_float_path, cv::Mat::eye(3, 3, CV_32F), cv::Mat::zeros(5, 1, CV_32F));
    loaded = calib::file::loadMonoCalibrationFile(valid_float_path, error);
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

cv::Mat makeSyntheticCheckerboard(int corners_x, int corners_y, int square_pixels = 60)
{
    cv::Mat board((corners_y + 1) * square_pixels, (corners_x + 1) * square_pixels, CV_8UC1, cv::Scalar(255));
    for (int y = 0; y <= corners_y; ++y)
        for (int x = 0; x <= corners_x; ++x)
            if ((x + y) % 2 == 0)
                cv::rectangle(board, {x * square_pixels, y * square_pixels, square_pixels, square_pixels},
                              cv::Scalar(0), cv::FILLED);
    return board;
}

void testMonoBoardConfigAndCornerDetection()
{
    video::CameraManager cameras;
    video::CameraService camera_service{cameras};
    headless::HeadlessCommandMapper mapper{camera_service};
    auto message = messageWithId();
    message.image_folder = "images"; message.output_file = "mono.yml";
    message.board_corners_x = 10; message.board_corners_y = 7; message.square_size_mm = 12.5;
    auto mapped = mapper.mapMonoCalibrate(message);
    requireCameraProjector(mapped.ok, "mono calibration with explicit BoardConfig did not map");
    const auto command = std::get<cmd::CmdCalibrate>(*mapped.command);
    requireCameraProjector(command.board_corners_x == 10 && command.board_corners_y == 7 &&
                           command.square_size_mm == 12.5, "mono BoardConfig changed during mapping");
    message.board_corners_x = 0;
    requireCameraProjector(!mapper.mapMonoCalibrate(message).ok, "zero board_corners_x was accepted");
    message.board_corners_x = 10; message.board_corners_y = -1;
    requireCameraProjector(!mapper.mapMonoCalibrate(message).ok, "negative board_corners_y was accepted");
    message.board_corners_y = 7; message.square_size_mm = 0.0;
    requireCameraProjector(!mapper.mapMonoCalibrate(message).ok, "zero square_size_mm was accepted");

    auto preview = messageWithId();
    preview.role = "left"; preview.output = "preview.png";
    preview.board_corners_x = 10; preview.board_corners_y = 7; preview.square_size_mm = 12.5;
    const auto unopened = mapper.mapDetectCalibrationCorners(preview);
    requireCameraProjector(!unopened.ok && unopened.error->code == "camera_not_open",
                           "corner preview did not resolve the camera role");
    preview.output.reset();
    requireCameraProjector(mapper.mapDetectCalibrationCorners(preview).error->code == "missing_field",
                           "corner preview accepted a missing output");

    calib::Calibrator calibrator;
    calibrator.setBoardConfig({{10, 7}, 12.5F});
    cv::Mat rendered; std::vector<cv::Point2f> corners;
    const auto checkerboard = makeSyntheticCheckerboard(10, 7);
    requireCameraProjector(calibrator.detectAndDraw(checkerboard, rendered, corners),
                           "synthetic checkerboard was not detected");
    requireCameraProjector(corners.size() == 70 && !rendered.empty(),
                           "synthetic checkerboard corner count/preview is invalid");
    cv::Mat blank(checkerboard.size(), CV_8UC1, cv::Scalar(127));
    requireCameraProjector(!calibrator.detectAndDraw(blank, rendered, corners),
                           "blank image was reported as a checkerboard");
}

void testMonoCalibrationArtifactAndCameraProjectorCompatibility()
{
    const auto directory = testTempDir("mono_calibration_board_config");
    const auto images = directory / "images";
    std::filesystem::create_directories(images);
    const cv::Mat board = makeSyntheticCheckerboard(10, 7, 50);
    const cv::Mat camera_K = (cv::Mat_<double>(3, 3) << 760.0, 0.0, 440.0, 0.0, 755.0, 320.0, 0.0, 0.0, 1.0);
    const std::array<cv::Vec3d, 6> rotations{{
        {0.02, -0.04, 0.01}, {0.12, -0.18, 0.04}, {-0.15, 0.12, -0.06},
        {0.20, 0.08, 0.10}, {-0.08, -0.22, -0.12}, {0.16, -0.10, 0.18}}};
    const std::array<cv::Vec3d, 6> translations{{
        {-55.0, -38.0, 235.0}, {-62.0, -42.0, 255.0}, {-48.0, -35.0, 225.0},
        {-58.0, -45.0, 270.0}, {-45.0, -32.0, 245.0}, {-65.0, -40.0, 260.0}}};
    const std::vector<cv::Point3f> outer{{-12.5F, -12.5F, 0.0F}, {125.0F, -12.5F, 0.0F},
                                         {125.0F, 87.5F, 0.0F}, {-12.5F, 87.5F, 0.0F}};
    const std::array<cv::Point2f, 4> source{{{0, 0}, {549, 0}, {549, 399}, {0, 399}}};
    for (std::size_t i = 0; i < rotations.size(); ++i)
    {
        std::vector<cv::Point2f> destination;
        cv::projectPoints(outer, rotations[i], translations[i], camera_K, cv::noArray(), destination);
        cv::Mat image(640, 880, CV_8UC1, cv::Scalar(255));
        cv::warpPerspective(board, image, cv::getPerspectiveTransform(source.data(), destination.data()), image.size(),
                            cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(255));
        requireCameraProjector(cv::imwrite((images / ("frame_" + std::to_string(i) + ".png")).string(), image),
                               "failed to write synthetic mono calibration image");
    }
    video::CameraManager cameras;
    calib::Calibrator calibrator;
    const auto output = directory / "mono_left.yml";
    const auto result = calib::calibrate(cameras, &calibrator,
        {video::kInvalidCameraId, images.string(), output.string(), {}, false, 10, 7, 12.5});
    requireCameraProjector(result.ok && std::isfinite(result.rms), "synthetic mono calibration failed");

    cv::FileStorage storage(output.string(), cv::FileStorage::READ);
    int board_x = 0, board_y = 0, image_width = 0, image_height = 0;
    double square_mm = 0.0; cv::Mat K, D;
    storage["board_corners_x"] >> board_x; storage["board_corners_y"] >> board_y;
    storage["square_size_mm"] >> square_mm; storage["image_width"] >> image_width;
    storage["image_height"] >> image_height; storage["K"] >> K; storage["D"] >> D;
    requireCameraProjector(board_x == 10 && board_y == 7 && square_mm == 12.5,
                           "mono calibration artifact lost BoardConfig");
    requireCameraProjector(image_width == 880 && image_height == 640 && K.rows == 3 && K.cols == 3 && !D.empty(),
                           "mono calibration artifact is incomplete");
    std::string load_error;
    const auto loaded = calib::file::loadMonoCalibrationFile(output, load_error);
    requireCameraProjector(loaded && loaded->image_width == 880 && loaded->image_height == 640,
                           "Camera-Projector mono calibration loader rejected the artifact");
}

void testScanDatasetValidator()
{
    scan::dataset::ScanDatasetValidator validator;

    auto dir = testTempDir("valid");
    writeValidScanDataset(dir, 2);
    auto result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(result.valid);
    assert(!result.partial);
    assert(result.scan_id == "session_001");
    assert(result.pattern_count == 2);
    assert(result.left_count == 2 && result.right_count == 2);
    assert(result.width == 20 && result.height == 16);
    assert(result.issues.empty());

    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir / "missing", false});
    assert(!result.valid && result.issues.size() == 1 && result.issues[0].code == "input_dir_not_found");

    dir = testTempDir("metadata_missing");
    std::filesystem::create_directories(dir / "left");
    std::filesystem::create_directories(dir / "right");
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid && result.issues[0].code == "pattern_count_not_found");

    dir = testTempDir("invalid_pattern_count");
    writeValidScanDataset(dir, 1);
    writeScanMetadata(dir, 0);
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "metadata_invalid_pattern_count"; }));

    dir = testTempDir("invalid_surface");
    writeValidScanDataset(dir, 1);
    writeScanMetadata(dir, 1, 0, 16);
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "metadata_invalid_surface"; }));

    dir = testTempDir("left_missing");
    writeValidScanDataset(dir, 1);
    std::filesystem::remove_all(dir / "left");
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "left_dir_not_found"; }));

    dir = testTempDir("right_missing");
    writeValidScanDataset(dir, 1);
    std::filesystem::remove_all(dir / "right");
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "right_dir_not_found"; }));

    dir = testTempDir("missing_left_image");
    writeValidScanDataset(dir, 2);
    std::filesystem::remove(patternPath(dir, "left", 1));
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid && result.partial && result.missing_count == 1);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "missing_left_image"; }));
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, true});
    assert(result.valid && result.partial);

    dir = testTempDir("missing_right_image");
    writeValidScanDataset(dir, 2);
    std::filesystem::remove(patternPath(dir, "right", 1));
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid && result.partial && result.missing_count == 1);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "missing_right_image"; }));

    dir = testTempDir("unreadable");
    writeValidScanDataset(dir, 1);
    std::ofstream(patternPath(dir, "left", 0)) << "not a png";
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "unreadable_left_image"; }));

    dir = testTempDir("stereo_size_mismatch");
    writeValidScanDataset(dir, 1);
    writeImage(patternPath(dir, "right", 0), 24, 16);
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "stereo_size_mismatch"; }));

    dir = testTempDir("image_size_inconsistent");
    writeValidScanDataset(dir, 2);
    writeImage(patternPath(dir, "left", 1), 22, 16);
    writeImage(patternPath(dir, "right", 1), 22, 16);
    result = validator.validate(scan::dataset::ScanDatasetValidationConfig{dir, false});
    assert(!result.valid);
    assert(std::any_of(result.issues.begin(), result.issues.end(),
                       [](const auto& issue) { return issue.code == "image_size_inconsistent"; }));
}

void testScanDatasetHandler()
{
    CommandRuntime runtime;
    const auto dir = testTempDir("handler");
    writeValidScanDataset(dir, 1);

    auto result = runtime.executor.execute(
        cmd::Command{cmd::CmdValidateScanDataset{dir.string(), false, {}, {}, {}}});
    assert(result.handled && result.ok);
    assert(result.values.at("valid") == "true");
    assert(result.values.at("issues_json") == "[]");
}

void testDecodeServiceSyntheticDataset()
{
    scan::dataset::ScanDatasetValidator validator;
    decode::DecodeService decode_service{validator};

    const auto input_dir = testTempDir("decode_synthetic_input");
    const auto output_dir = testTempDir("decode_synthetic_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);

    auto result =
        decode_service.decodePatterns(decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
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
    result = decode_service.decodePatterns(
        decode::DecodePatternsConfig{input_dir, high_threshold_output, 300, false});
    assert(result.ok);
    assert(result.left_valid_count == 0 && result.right_valid_count == 0);
}

void testPhotodiodeMarkerRequiresProjectorMargin()
{
    projector::ProjectorSurface surface;
    surface.surface_width = 16;
    surface.surface_height = 12;
    surface.pattern_width = 16;
    surface.pattern_height = 12;
    assert(!projector::canPlacePhotodiodeMarker(surface));
    surface.surface_width = 48;
    assert(projector::canPlacePhotodiodeMarker(surface));
    surface.surface_width = 80;
    surface.pattern_x = 32;
    assert(projector::canPlacePhotodiodeMarker(surface));
    assert(projector::photodiodeMarkerValue(0) == 0);
    assert(projector::photodiodeMarkerValue(1) == 255);

    surface.surface_width = 9;
    surface.surface_height = 10;
    surface.pattern_x = 8;
    surface.pattern_y = 9;
    surface.pattern_width = 1;
    surface.pattern_height = 1;
    assert(!projector::canPlacePhotodiodeMarker(surface));
}

void testPhotodiodeScanConfigValidation()
{
    FakeWindowBackend backend;
    auto monitor_service = fakeMonitorService();
    win::WindowService window_service{backend};
    projector::ProjectorService projector_service{window_service, monitor_service};
    video::CameraManager cameras;
    capture::CaptureService capture_service{cameras};
    video::CameraService camera_service{cameras};
    scan::ScanEventQueue events;
    scan::ScanService scan_service{projector_service, capture_service, camera_service, events};

    scan::ScanStartConfig config;
    config.projector_role = "projector";
    config.left_role = "left";
    config.right_role = "right";
    config.output_dir = testTempDir("photodiode_scan_validation");
    config.photodiode_device.clear();
    auto result = scan_service.startScan(config);
    assert(!result.ok && result.error->code == "projector_not_open");

    config.sync_mode = scan::ScanSyncMode::photodiode;
    result = scan_service.startScan(config);
    assert(!result.ok && result.error->code == "invalid_scan_config");
    config.photodiode_device = "/dev/ttyUSB0";
    result = scan_service.startScan(config);
    assert(!result.ok && result.error->code == "projector_not_open");
}

void testDecodeServiceFailures()
{
    scan::dataset::ScanDatasetValidator validator;
    decode::DecodeService decode_service{validator};

    auto input_dir = testTempDir("decode_invalid_dataset");
    auto output_dir = testTempDir("decode_invalid_output");
    auto result =
        decode_service.decodePatterns(decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
    assert(!result.ok && result.error->code == "scan_dataset_invalid");

    input_dir = testTempDir("decode_count_mismatch");
    output_dir = testTempDir("decode_count_mismatch_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);
    writeScanMetadata(input_dir, 9, 8, 4);
    result = decode_service.decodePatterns(decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
    assert(!result.ok && result.error->code == "decode_pattern_count_mismatch");

    input_dir = testTempDir("decode_write_failure");
    output_dir = testTempDir("decode_write_failure_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);
    std::filesystem::remove_all(output_dir);
    std::ofstream(output_dir) << "not a directory";
    result = decode_service.decodePatterns(decode::DecodePatternsConfig{input_dir, output_dir, 15, false});
    assert(!result.ok && result.error->code == "decode_output_write_failed");
}

void testDecodeHandler()
{
    CommandRuntime runtime;
    const auto input_dir = testTempDir("decode_handler_input");
    const auto output_dir = testTempDir("decode_handler_output");
    writeSyntheticGrayCodeDataset(input_dir, 8, 4);

    auto result = runtime.executor.execute(
        cmd::Command{cmd::CmdDecodePatterns{
            input_dir.string(), output_dir.string(), 15, false, {}, {}, {}, std::nullopt, std::nullopt, std::nullopt}});
    assert(result.handled && result.ok);
    assert(result.values.at("projector_width") == "8");
    assert(result.values.at("projector_height") == "4");
    assert(result.values.at("left_valid_count") == "32");
}

void testServiceValidation()
{
    video::CameraManager cameras;
    video::CameraService service{cameras};
    assert(!service.resolveCameraId("left"));
    const auto invalid = service.openCamera(0, "");
    assert(!invalid.ok && invalid.error->code == "invalid_command");
    const auto close = service.closeCamera("left");
    assert(!close.ok && close.error->code == "camera_not_open");
}

void testCommandExecutorDomainRouting()
{
    CommandRuntime runtime;

    auto result = runtime.executor.execute(cmd::Command{cmd::CmdCaptureFrame{}});
    assert(result.handled && !result.ok);

    result = runtime.executor.execute(cmd::Command{cmd::CmdScanStatus{"missing"}});
    assert(result.handled && !result.ok);

    const auto missing = testTempDir("executor_missing") / "missing";
    result = runtime.executor.execute(cmd::Command{cmd::CmdCalibrate{
        video::kInvalidCameraId, missing.string(), (missing / "mono.yml").string(), {}, false}});
    assert(result.handled && !result.ok);

    cmd::CmdStereoCalibrate stereo;
    stereo.left_calibration_file = (missing / "left.yml").string();
    stereo.right_calibration_file = (missing / "right.yml").string();
    stereo.output_file = (missing / "stereo.yml").string();
    result = runtime.executor.execute(cmd::Command{stereo});
    assert(result.handled && !result.ok);

    result = runtime.executor.execute(cmd::Command{cmd::CmdValidateReconstruction{
        missing, missing / "calibration.yml", {}}});
    assert(result.handled && !result.ok);

    result = runtime.executor.execute(cmd::Command{cmd::CmdCameraProjectorCalibrate{
        missing, missing / "mono.yml", missing / "camera-projector.yml", 10, 7, 12.5, 1.0, 2.0, false}});
    requireCameraProjector(result.handled && !result.ok, "Camera-Projector Executor path did not return failure");
}

void testCameraProjectorMapper()
{
    video::CameraManager cameras; video::CameraService camera_service{cameras};
    headless::HeadlessCommandMapper mapper{camera_service};
    auto message=messageWithId(); message.observations_dir="observations"; message.camera_calibration_file="mono.yml";
    message.output_file="camera-projector.yml"; message.board_corners_x=10; message.board_corners_y=7;
    message.square_size_mm=12.5; message.max_mean_displacement_px=1.0; message.max_corner_displacement_px=2.0;
    auto mapped=mapper.mapCameraProjectorCalibrate(message);
    requireCameraProjector(mapped.ok, "valid Camera-Projector command did not map");
    const auto command=std::get<cmd::CmdCameraProjectorCalibrate>(*mapped.command);
    requireCameraProjector(!command.overwrite && command.square_size_mm==12.5, "mapped values differ");
    message.overwrite=true; mapped=mapper.mapCameraProjectorCalibrate(message);
    requireCameraProjector(std::get<cmd::CmdCameraProjectorCalibrate>(*mapped.command).overwrite,
                           "overwrite was not mapped");
    message.square_size_mm=0.0;
    requireCameraProjector(!mapper.mapCameraProjectorCalibrate(message).ok, "zero square size was accepted");
    message.square_size_mm=12.5; message.board_corners_x.reset();
    requireCameraProjector(!mapper.mapCameraProjectorCalibrate(message).ok, "missing board size was accepted");
    message.board_corners_x=3; message.board_corners_y=3;
    requireCameraProjector(!mapper.mapCameraProjectorCalibrate(message).ok, "undersized board was accepted");
}

} // namespace

int main()
{
    testStructuredLightPatternGeneration();
    testMapper();
    testWindowMapper();
    testProjectorMapper();
    testMonitorService();
    testMonitorFallbackAcrossServices();
    testScanMapper();
    testStereoScanMapper();
    testHandler();
    testWindowHandler();
    testProjectorLogicalResolutionSeparateFromDisplay();
    testProjectorSameResolutionStillGeneratesExpectedCount();
    testProjectorSurfaceReconfigurationKeepsGeneratedPatterns();
    testProjectorSurfaceChangesBeforeGenerateKeepCodeResolution();
    testProjectorNearestResizeProducesBinaryValues();
    testProjectorHandlerReportsCodeAndDisplayResolution();
    testScanMetadataDistinguishesCodeAndDisplayResolution();
    testProjectorHandler();
    testScanDatasetResolver();
    testMonoCalibrationFileLoader();
    testMonoBoardConfigAndCornerDetection();
    testMonoCalibrationArtifactAndCameraProjectorCompatibility();
    testScanDatasetHandler();
    testDecodeHandler();
    testCommandExecutorDomainRouting();
    testCameraProjectorMapper();
    testProjectorSurfaceConfiguration();
    testServiceValidation();
    testWindowServiceValidation();
    testWindowPostOpenAction();
    testProjectorServiceValidation();
    testScanDatasetValidator();
    testDecodeServiceSyntheticDataset();
    testPhotodiodeMarkerRequiresProjectorMargin();
    testPhotodiodeScanConfigValidation();
    testDecodeServiceFailures();
    return 0;
}
