#include "scan/scan_event.hpp"
#include "stereo_scan/stereo_scan_service.hpp"
#include "window/window_placement_service.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
using stereo_scan::EnsureResult;
using stereo_scan::MonitorSizeResult;
using stereo_scan::OperationResult;
using stereo_scan::ReconstructionOutput;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

class FakeBackend final : public stereo_scan::StereoScanBackend
{
  public:
    EnsureResult ensureCamera(const std::string& role, int device) override
    {
        ++ensure_camera_calls[role];
        const auto it = cameras.find(role);
        if (it != cameras.end())
        {
            if (it->second != device)
                return {{false, "camera_role_device_conflict", "conflict"}, false, {}};
            return {{true, {}, {}}, false, {}};
        }
        cameras[role] = device;
        ++camera_open_count[role];
        return {{true, {}, {}}, true, {}};
    }
    EnsureResult ensureStream(const std::string& role) override
    {
        ++ensure_stream_calls[role];
        const auto it = streams.find(role);
        if (it != streams.end())
            return {{true, {}, {}}, false, it->second};
        auto url = "http://test/" + role + ".mjpg";
        streams[role] = url;
        ++stream_start_count[role];
        return {{true, {}, {}}, true, url};
    }
    MonitorSizeResult monitorSize(int index) override
    {
        monitor_index = index;
        return monitor_ok ? MonitorSizeResult{{true, {}, {}}, monitor_width, monitor_height}
                          : MonitorSizeResult{{false, "monitor_not_found", "missing"}, 0, 0};
    }
    OperationResult openWindow(const cmd::CmdStereoScan& command, int width, int height) override
    {
        ++window_open_count;
        last_display_width = width;
        last_display_height = height;
        last_window_monitor_index = command.monitor_index;
        if (fail_window)
            return OperationResult::failure("window_open_failed", "fake");
        window_open = true;
        return OperationResult::success();
    }
    OperationResult openProjector(const cmd::CmdStereoScan&) override
    {
        ++projector_open_count;
        projector_open = true;
        return OperationResult::success();
    }
    OperationResult configureSurface(const cmd::CmdStereoScan&, int width, int height) override
    {
        surface_width = width;
        surface_height = height;
        return OperationResult::success();
    }
    OperationResult generatePatterns(const cmd::CmdStereoScan&) override
    {
        ++pattern_count;
        return OperationResult::success();
    }
    OperationResult runScan(const cmd::CmdStereoScan&, const std::filesystem::path&, std::stop_token token) override
    {
        ++scan_count;
        while (block_scan && !token.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return block_scan ? OperationResult::failure("scan_stopped", "stopped") : OperationResult::success();
    }
    OperationResult decode(const cmd::CmdStereoScan&, const std::filesystem::path&,
                           const std::filesystem::path&) override
    {
        ++decode_count;
        return OperationResult::success();
    }
    ReconstructionOutput reconstruct(const cmd::CmdStereoScan& command, const std::filesystem::path&) override
    {
        ++reconstruction_count;
        std::ofstream(command.ply_file) << "ply\n";
        return {{true, {}, {}}, command.ply_file, 182493};
    }
    void closeProjector(const std::string&) override
    {
        if (projector_open)
        {
            projector_open = false;
            ++projector_close_count;
        }
    }
    void closeWindow(const std::string&) override
    {
        if (window_open)
        {
            window_open = false;
            ++window_close_count;
        }
    }
    void stopStream(const std::string& role) override
    {
        streams.erase(role);
        ++stream_stop_count[role];
    }
    void closeCamera(const std::string& role) override
    {
        cameras.erase(role);
        ++camera_close_count[role];
    }
    void requestStopScan(const std::string&) override
    {
        ++stop_scan_count;
    }

    std::map<std::string, int> cameras, ensure_camera_calls, camera_open_count, camera_close_count;
    std::map<std::string, std::string> streams;
    std::map<std::string, int> ensure_stream_calls, stream_start_count, stream_stop_count;
    int monitor_width{1920}, monitor_height{1080}, monitor_index{-1};
    bool monitor_ok{true};
    bool fail_window{false}, block_scan{false}, window_open{false}, projector_open{false};
    int window_open_count{}, window_close_count{}, projector_open_count{}, projector_close_count{};
    int last_display_width{}, last_display_height{}, surface_width{}, surface_height{};
    int last_window_monitor_index{-1};
    int pattern_count{}, scan_count{}, decode_count{}, reconstruction_count{}, stop_scan_count{};
};

std::filesystem::path tempRoot(const std::string& name)
{
    auto root = std::filesystem::temp_directory_path() / ("stereo_scan_service_" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}
void writeCalibration(const std::filesystem::path& path)
{
    cv::FileStorage s(path.string(), cv::FileStorage::WRITE);
    const auto k = cv::Mat::eye(3, 3, CV_64F);
    const auto d = cv::Mat::zeros(1, 5, CV_64F);
    const auto r = cv::Mat::eye(3, 3, CV_64F);
    cv::Mat t = (cv::Mat_<double>(3, 1) << 100, 0, 0);
    s << "K1" << k << "D1" << d << "K2" << k << "D2" << d << "R" << r << "T" << t << "image_width" << 640
      << "image_height" << 480;
}
cmd::CmdStereoScan commandFor(const std::filesystem::path& root, const std::string& id)
{
    cmd::CmdStereoScan command;
    command.scan_id = id;
    command.monitor_index = 1;
    command.calibration_file = root / "stereo.yml";
    command.output_dir = root / id;
    command.ply_file = command.output_dir / "cloud.ply";
    return command;
}
void waitComplete(stereo_scan::StereoScanService& service)
{
    for (int i = 0; i < 500 && service.isActive(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(!service.isActive(), "stereo scan did not finish");
}
const scan::ScanEvent* findEvent(const std::vector<scan::ScanEvent>& events, const std::string& name)
{
    for (const auto& event : events)
        if (event.event == name)
            return &event;
    return nullptr;
}

void testSuccessAndRepeatedScan()
{
    const auto root = tempRoot("repeat");
    writeCalibration(root / "stereo.yml");
    FakeBackend backend;
    scan::ScanEventQueue events;
    stereo_scan::StereoScanService service{backend, events};
    auto first = service.start(commandFor(root, "scan1"));
    require(first.ok, "first scan rejected");
    waitComplete(service);
    auto first_events = events.drain();
    const auto* completed = findEvent(first_events, "stereo_scan_completed");
    require(completed && completed->values.at("point_count") == "182493" &&
                completed->values.at("left_stream_url") == "http://test/left.mjpg" &&
                completed->values.at("right_stream_url") == "http://test/right.mjpg" &&
                completed->values.contains("scan_dir") && completed->values.contains("decode_dir") &&
                completed->values.contains("ply_file"),
            "completed event contract differs");
    require(!backend.window_open && !backend.projector_open && backend.streams.size() == 2 &&
                backend.cameras.size() == 2,
            "success resource contract differs");
    auto second = service.start(commandFor(root, "scan2"));
    require(second.ok, "second scan rejected");
    waitComplete(service);
    require(backend.camera_open_count["left"] == 1 && backend.camera_open_count["right"] == 1,
            "camera was reopened on second scan");
    require(backend.stream_start_count["left"] == 1 && backend.stream_start_count["right"] == 1,
            "stream was restarted on second scan");
    require(backend.window_open_count == 2 && backend.window_close_count == 2 && backend.projector_open_count == 2 &&
                backend.projector_close_count == 2,
            "scan-only resources were not recreated and released");
    require(backend.last_window_monitor_index == 1, "stereo scan did not pass monitor_index to window placement");
    require(backend.pattern_count == 2 && backend.scan_count == 2 && backend.decode_count == 2 &&
                backend.reconstruction_count == 2,
            "orchestration stages were not executed twice");
    require(std::filesystem::exists(root / "scan1/cloud.ply") && std::filesystem::exists(root / "scan2/cloud.ply"),
            "PLY files missing");
}

void testReuseConflictAndPartialCleanup()
{
    const auto root = tempRoot("ownership");
    writeCalibration(root / "stereo.yml");
    FakeBackend backend;
    backend.cameras = {{"left", 0}, {"right", 2}};
    backend.streams = {{"left", "existing-left"}, {"right", "existing-right"}};
    scan::ScanEventQueue events;
    stereo_scan::StereoScanService service{backend, events};
    auto reused = service.start(commandFor(root, "reuse"));
    require(reused.ok, "existing resources not reused");
    require(reused.values.at("left_stream_url") == "existing-left" &&
                reused.values.at("right_stream_url") == "existing-right",
            "existing stream URL not returned");
    waitComplete(service);
    require(backend.camera_open_count.empty() && backend.stream_start_count.empty(), "existing resource recreated");

    auto conflict_command = commandFor(root, "conflict");
    conflict_command.left_camera_id = 4;
    auto conflict = service.start(conflict_command);
    require(!conflict.ok && conflict.error->code == "camera_role_device_conflict", "camera conflict not reported");
    require(backend.cameras.at("left") == 0 && backend.streams.at("left") == "existing-left",
            "conflict damaged stream");

    FakeBackend partial;
    partial.cameras["left"] = 0;
    partial.streams["left"] = "existing-left";
    partial.fail_window = true;
    scan::ScanEventQueue partial_events;
    stereo_scan::StereoScanService partial_service{partial, partial_events};
    auto started = partial_service.start(commandFor(root, "partial"));
    require(started.ok, "partial scan did not start");
    waitComplete(partial_service);
    require(partial.cameras.contains("left") && partial.streams.contains("left"), "existing left resource cleaned");
    require(!partial.cameras.contains("right") && !partial.streams.contains("right"), "owned right resource leaked");
    require(partial.camera_close_count["right"] == 1 && partial.stream_stop_count["right"] == 1,
            "owned right resource cleanup missing");
}

void testDisplayDefaultsAndOverride()
{
    const auto root = tempRoot("display");
    writeCalibration(root / "stereo.yml");
    FakeBackend backend;
    scan::ScanEventQueue events;
    stereo_scan::StereoScanService service{backend, events};
    auto command = commandFor(root, "default");
    require(service.start(command).ok, "default display scan rejected");
    waitComplete(service);
    require(command.code_width == 480 && command.code_height == 270 && backend.last_display_width == 1920 &&
                backend.last_display_height == 1080 && backend.surface_width == 1920 && backend.surface_height == 1080,
            "monitor-size display default not applied");
    command = commandFor(root, "override");
    command.display_width = 1728;
    command.display_height = 1080;
    require(service.start(command).ok, "display override rejected");
    waitComplete(service);
    require(backend.last_display_width == 1728 && backend.last_display_height == 1080, "display override not applied");
}

void testNiriIdentityAndFactoryMapping()
{
    const std::string json = R"([{"id":7,"title":"Projector","pid":123},{"id":8,"title":"Other","pid":123}])";
    const auto windows = win::parseNiriWindows(json);
    std::string id;
    require(win::resolveNiriWindow(windows, 123, "Projector", id).ok && id == "7",
            "niri unique identity mapping failed");
    auto ambiguous = windows;
    ambiguous.push_back({"9", "Projector", 123});
    require(win::resolveNiriWindow(ambiguous, 123, "Projector", id).error_code == "window_native_identity_ambiguous",
            "ambiguous niri identity was accepted");
    require(win::selectWindowPlacementBackend({"wayland", true, false}) == win::WindowPlacementBackendKind::Niri,
            "niri factory selection failed");
    require(win::selectWindowPlacementBackend({"x11", false, true}) == win::WindowPlacementBackendKind::X11,
            "X11 factory selection failed");
    require(win::selectWindowPlacementBackend({"wayland", false, false}) ==
                win::WindowPlacementBackendKind::Unsupported,
            "unknown Wayland selection failed");
    const auto monitors = win::parseNiriMonitorOutput(
        R"({"DP-1":{"name":"DP-1","logical":{"x":1920,"y":0,"width":1920,"height":1080,"scale":1.0}},"eDP-1":{"name":"eDP-1","logical":{"x":0,"y":0,"width":1920,"height":1080,"scale":1.0}}})");
    require(monitors.size() == 2 && monitors[0].name == "eDP-1" && monitors[1].name == "DP-1" &&
                monitors[1].monitor_index == 1,
            "niri monitor index to output mapping failed");
}

void testCalibrationStageAndShutdown()
{
    const auto root = tempRoot("shutdown");
    FakeBackend backend;
    scan::ScanEventQueue events;
    stereo_scan::StereoScanService service{backend, events};
    auto missing = service.start(commandFor(root, "missing"));
    require(!missing.ok, "missing calibration accepted");
    const auto failed = events.drain();
    const auto* event = findEvent(failed, "stereo_scan_failed");
    require(event && event->values.at("stage") == "calibration", "calibration failure stage differs");

    writeCalibration(root / "stereo.yml");
    backend.block_scan = true;
    require(service.start(commandFor(root, "blocked")).ok, "blocking scan rejected");
    for (int i = 0; i < 100 && backend.scan_count == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    service.shutdown();
    require(!service.isActive() && backend.stop_scan_count > 0, "shutdown did not stop and clear active scan");
}
} // namespace

int main()
{
    testSuccessAndRepeatedScan();
    testReuseConflictAndPartialCleanup();
    testDisplayDefaultsAndOverride();
    testNiriIdentityAndFactoryMapping();
    testCalibrationStageAndShutdown();
}
