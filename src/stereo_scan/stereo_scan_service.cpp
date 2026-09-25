#include "stereo_scan/stereo_scan_service.hpp"

#include "decode/decode_service.hpp"
#include "projector/projector_service.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "scan/scan_event.hpp"
#include "scan/scan_service.hpp"
#include "video/camera_service.hpp"
#include "window/monitor_service.hpp"
#include "window/window_service.hpp"

#include <chrono>
#include <filesystem>
#include <opencv2/core.hpp>
#include <thread>
#include <utility>

namespace stereo_scan
{
namespace
{
OperationResult operationFailure(const std::string& fallback_code, const std::string& fallback_message,
                                 const auto& error)
{
    return OperationResult::failure(error ? error->code : fallback_code,
                                    error ? error->message : fallback_message);
}

class DomainStereoScanBackend final : public StereoScanBackend
{
  public:
    DomainStereoScanBackend(video::CameraService& cameras, win::WindowService& windows,
                            win::MonitorService& monitors, projector::ProjectorService& projectors,
                            scan::ScanService& scans, decode::DecodeService& decoder,
                            reconstruction::ReconstructionService& reconstruction,
                            StreamOperation start_stream, StreamOperation stop_stream, StreamLookup stream_lookup)
        : cameras_(cameras), windows_(windows), monitors_(monitors), projectors_(projectors), scans_(scans),
          decoder_(decoder), reconstruction_(reconstruction), start_stream_(std::move(start_stream)),
          stop_stream_(std::move(stop_stream)), stream_lookup_(std::move(stream_lookup)) {}

    EnsureResult ensureCamera(const std::string& role, int device_index) override
    {
        if (const auto current = cameras_.resolveDeviceIndex(role))
        {
            if (*current != device_index)
                return {{false, "camera_role_device_conflict",
                         "camera role " + role + " is already bound to device " + std::to_string(*current)},
                        false, {}};
            return {{true, {}, {}}, false, {}};
        }
        const auto result = cameras_.openCamera(device_index, role);
        if (!result.ok)
            return {{false, result.error ? result.error->code : "camera_open_failed",
                     result.error ? result.error->message : "camera open failed"}, false, {}};
        return {{true, {}, {}}, true, {}};
    }

    EnsureResult ensureStream(const std::string& role) override
    {
        if (const auto existing = stream_lookup_(role)) return {{true, {}, {}}, false, *existing};
        const auto result = start_stream_(role);
        if (!result.ok)
            return {{false, result.error ? result.error->code : "stream_start_failed",
                     result.error ? result.error->message : "stream start failed"}, false, {}};
        const auto it = result.values.find("url");
        return {{true, {}, {}}, true, it == result.values.end() ? std::string{} : it->second};
    }

    MonitorSizeResult monitorSize(int monitor_index) override
    {
        const auto monitor = monitors_.getMonitor(monitor_index);
        if (!monitor) return {{false, "monitor_not_found", "monitor index is unavailable"}, 0, 0};
        return {{true, {}, {}}, monitor->width, monitor->height};
    }

    OperationResult openWindow(const cmd::CmdStereoScan& command, int width, int height) override
    {
        const auto result = windows_.openWindow({command.window_role, "Projector", width, height,
            command.monitor_index, true, command.post_open_key, command.post_open_action});
        return result.ok ? OperationResult::success()
                         : operationFailure("window_open_failed", "window open failed", result.error);
    }

    OperationResult openProjector(const cmd::CmdStereoScan& command) override
    {
        const auto result = projectors_.openProjector({command.projector_role, command.window_role,
                                                       command.code_width, command.code_height});
        return result.ok ? OperationResult::success()
                         : operationFailure("projector_open_failed", "projector open failed", result.error);
    }

    OperationResult configureSurface(const cmd::CmdStereoScan& command, int width, int height) override
    {
        const auto result = projectors_.configureSurface({command.projector_role, command.monitor_index, width, height,
            command.x, command.y, command.placement == "custom" ? projector::ProjectorPlacement::custom
                                                                  : projector::ProjectorPlacement::center});
        return result.ok ? OperationResult::success()
                         : operationFailure("projector_surface_failed", "surface configuration failed", result.error);
    }

    OperationResult generatePatterns(const cmd::CmdStereoScan& command) override
    {
        const auto result = projectors_.generatePatterns(command.projector_role);
        return result.ok ? OperationResult::success()
                         : operationFailure("pattern_generate_failed", "pattern generation failed", result.error);
    }

    OperationResult runScan(const cmd::CmdStereoScan& command, const std::filesystem::path& scan_dir,
                            std::stop_token token) override
    {
        scan::ScanStartConfig config;
        config.scan_id = command.scan_id;
        config.projector_role = command.projector_role;
        config.left_role = command.left_role;
        config.right_role = command.right_role;
        config.output_dir = scan_dir;
        config.sync_mode = command.sync_mode == "photodiode" ? scan::ScanSyncMode::photodiode
                                                              : scan::ScanSyncMode::delay;
        config.delay_ms = command.delay_ms;
        config.photodiode_device = command.photodiode_device;
        config.photodiode_baud = command.photodiode_baud;
        config.sync_timeout_ms = command.sync_timeout_ms;
        config.sync_guard_ms = command.guard_ms;
        const auto started = scans_.startScan(config);
        if (!started.ok)
            return OperationResult::failure(started.error ? started.error->code : "scan_failed",
                                            started.error ? started.error->message : "scan start failed");
        while (!token.stop_requested())
        {
            const auto status = scans_.scanStatus(command.scan_id);
            if (!status.ok)
                return OperationResult::failure(status.error ? status.error->code : "scan_failed",
                                                status.error ? status.error->message : "scan status failed");
            if (status.status == scan::ScanState::completed) return OperationResult::success();
            if (status.status == scan::ScanState::failed || status.status == scan::ScanState::stopped)
                return OperationResult::failure(status.error ? status.error->code : "scan_failed",
                                                status.error ? status.error->message : "scan did not complete");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        (void)scans_.stopScan(command.scan_id);
        return OperationResult::failure("scan_stopped", "stereo scan stopped");
    }

    OperationResult decode(const cmd::CmdStereoScan& command, const std::filesystem::path& scan_dir,
                           const std::filesystem::path& decode_dir) override
    {
        decode::DecodePatternsConfig config;
        config.input_dir = scan_dir;
        config.output_dir = decode_dir;
        config.threshold = command.decode_threshold;
        const auto result = decoder_.decodePatterns(config);
        return result.ok ? OperationResult::success()
                         : OperationResult::failure(result.error ? result.error->code : "decode_result_invalid",
                                                    result.error ? result.error->message : "decode failed");
    }

    ReconstructionOutput reconstruct(const cmd::CmdStereoScan& command,
                                      const std::filesystem::path& decode_dir) override
    {
        const auto result = reconstruction_.reconstruct({
            {decode_dir, command.calibration_file, {command.max_epipolar_error_px, {}, {}}},
            command.ply_file, false});
        if (!result.ok)
            return {{false, result.error ? result.error->code : "triangulation_failed",
                     result.error ? result.error->message : "reconstruction failed"}, {}, 0};
        return {{true, {}, {}}, result.output_file, result.point_count};
    }

    void closeProjector(const std::string& role) override { (void)projectors_.closeProjector(role); }
    void closeWindow(const std::string& role) override { (void)windows_.closeWindow(role); }
    void stopStream(const std::string& role) override { (void)stop_stream_(role); }
    void closeCamera(const std::string& role) override { (void)cameras_.closeCamera(role); }
    void requestStopScan(const std::string& scan_id) override { (void)scans_.stopScan(scan_id); }

  private:
    video::CameraService& cameras_;
    win::WindowService& windows_;
    win::MonitorService& monitors_;
    projector::ProjectorService& projectors_;
    scan::ScanService& scans_;
    decode::DecodeService& decoder_;
    reconstruction::ReconstructionService& reconstruction_;
    StreamOperation start_stream_;
    StreamOperation stop_stream_;
    StreamLookup stream_lookup_;
};

class InactiveGuard
{
  public:
    explicit InactiveGuard(std::function<void()> callback) : callback_(std::move(callback)) {}
    ~InactiveGuard() { callback_(); }
  private:
    std::function<void()> callback_;
};
}

StereoScanService::StereoScanService(
    video::CameraService& cameras, win::WindowService& windows, win::MonitorService& monitors,
    projector::ProjectorService& projectors, scan::ScanService& scans, decode::DecodeService& decoder,
    reconstruction::ReconstructionService& reconstruction, scan::ScanEventSink& events,
    StreamOperation start_stream, StreamOperation stop_stream, StreamLookup stream_lookup)
    : owned_backend_(std::make_unique<DomainStereoScanBackend>(cameras, windows, monitors, projectors, scans,
          decoder, reconstruction, std::move(start_stream), std::move(stop_stream), std::move(stream_lookup))),
      backend_(*owned_backend_), events_(events) {}

StereoScanService::StereoScanService(StereoScanBackend& backend, scan::ScanEventSink& events)
    : backend_(backend), events_(events) {}

StereoScanService::~StereoScanService() { shutdown(); }

common::CommandResult StereoScanService::start(const cmd::CmdStereoScan& command)
{
    std::unique_lock lock(mutex_);
    if (active_) return common::failure("stereo_scan_already_running", "a stereo scan is already running");
    if (worker_.joinable()) { lock.unlock(); worker_ = std::jthread{}; lock.lock(); }

    std::string code, message;
    if (!validateCalibration(command.calibration_file, code, message))
    {
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "calibration"},
             {"error_code", code}, {"error_message", message}});
        return common::failure(code, message);
    }
    if (command.display_width.has_value() != command.display_height.has_value())
        return common::failure("missing_field", "display_width and display_height must be specified together");
    const auto monitor = backend_.monitorSize(command.monitor_index);
    if (!monitor.ok) return common::failure(monitor.error_code, monitor.error_message);
    const int display_width = command.display_width.value_or(monitor.width);
    const int display_height = command.display_height.value_or(monitor.height);

    try
    {
        std::filesystem::create_directories(command.output_dir / "scan");
        std::filesystem::create_directories(command.output_dir / "decode");
        if (command.ply_file.has_parent_path()) std::filesystem::create_directories(command.ply_file.parent_path());
    }
    catch (const std::exception& error) { return common::failure("directory_create_failed", error.what()); }

    StereoScanOwnedResources owned;
    const auto left_camera = backend_.ensureCamera(command.left_role, command.left_camera_id);
    if (!left_camera.ok)
    {
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "camera"},
             {"error_code", left_camera.error_code}, {"error_message", left_camera.error_message}});
        return common::failure(left_camera.error_code, left_camera.error_message);
    }
    owned.opened_left_camera = left_camera.created;
    const auto right_camera = backend_.ensureCamera(command.right_role, command.right_camera_id);
    if (!right_camera.ok)
    {
        cleanup(command, owned, true);
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "camera"},
             {"error_code", right_camera.error_code}, {"error_message", right_camera.error_message}});
        return common::failure(right_camera.error_code, right_camera.error_message);
    }
    owned.opened_right_camera = right_camera.created;

    const auto left_stream = backend_.ensureStream(command.left_role);
    if (!left_stream.ok)
    {
        cleanup(command, owned, true);
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "stream"},
             {"error_code", left_stream.error_code}, {"error_message", left_stream.error_message}});
        return common::failure(left_stream.error_code, left_stream.error_message);
    }
    owned.started_left_stream = left_stream.created;
    const auto right_stream = backend_.ensureStream(command.right_role);
    if (!right_stream.ok)
    {
        cleanup(command, owned, true);
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "stream"},
             {"error_code", right_stream.error_code}, {"error_message", right_stream.error_message}});
        return common::failure(right_stream.error_code, right_stream.error_message);
    }
    owned.started_right_stream = right_stream.created;

    active_ = true;
    active_scan_id_ = command.scan_id;
    worker_ = std::jthread([this, command, left_url = left_stream.value, right_url = right_stream.value,
                            owned, display_width, display_height](std::stop_token token) mutable {
        InactiveGuard inactive([this] { setInactive(); });
        try { workerLoop(token, command, left_url, right_url, owned, display_width, display_height); }
        catch (const std::exception& error) { fail(command, owned, "scan", "internal_error", error.what(), true); }
        catch (...) { fail(command, owned, "scan", "internal_error", "unhandled stereo scan error", true); }
    });
    push("stereo_scan_started", {{"scan_id", command.scan_id}, {"left_stream_url", left_stream.value},
         {"right_stream_url", right_stream.value}, {"output_dir", command.output_dir.string()}});
    return common::success({{"scan_id", command.scan_id}, {"left_stream_url", left_stream.value},
        {"right_stream_url", right_stream.value}, {"output_dir", command.output_dir.string()},
        {"ply_file", command.ply_file.string()}});
}

void StereoScanService::workerLoop(std::stop_token token, cmd::CmdStereoScan command,
                                   std::string left_url, std::string right_url,
                                   StereoScanOwnedResources owned, int display_width, int display_height)
{
    auto result = backend_.openWindow(command, display_width, display_height);
    if (!result.ok) { fail(command, owned, "window", result.error_code, result.error_message, true); return; }
    owned.opened_window = true;
    result = backend_.openProjector(command);
    if (!result.ok) { fail(command, owned, "projector", result.error_code, result.error_message, true); return; }
    owned.opened_projector = true;
    result = backend_.configureSurface(command, display_width, display_height);
    if (!result.ok) { fail(command, owned, "projector", result.error_code, result.error_message, true); return; }
    result = backend_.generatePatterns(command);
    if (!result.ok) { fail(command, owned, "projector", result.error_code, result.error_message, true); return; }

    push("stereo_scan_scanning", {{"scan_id", command.scan_id}});
    result = backend_.runScan(command, command.output_dir / "scan", token);
    if (!result.ok) { fail(command, owned, "scan", result.error_code, result.error_message, true); return; }

    // Window/Projector are scan-only resources. Release them before file-only stages.
    backend_.closeProjector(command.projector_role); owned.opened_projector = false;
    backend_.closeWindow(command.window_role); owned.opened_window = false;

    push("stereo_scan_decoding", {{"scan_id", command.scan_id}});
    result = backend_.decode(command, command.output_dir / "scan", command.output_dir / "decode");
    if (!result.ok) { fail(command, owned, "decode", result.error_code, result.error_message, true); return; }
    push("stereo_scan_reconstructing", {{"scan_id", command.scan_id}});
    const auto reconstruction = backend_.reconstruct(command, command.output_dir / "decode");
    if (!reconstruction.ok)
    {
        fail(command, owned, "reconstruction", reconstruction.error_code, reconstruction.error_message, true);
        return;
    }
    // Camera and stream are intentionally retained after success, including resources created by this request.
    push("stereo_scan_completed", {{"scan_id", command.scan_id}, {"left_stream_url", left_url},
        {"right_stream_url", right_url}, {"scan_dir", (command.output_dir / "scan").string()},
        {"decode_dir", (command.output_dir / "decode").string()}, {"ply_file", reconstruction.ply_file.string()},
        {"point_count", std::to_string(reconstruction.point_count)}});
}

void StereoScanService::cleanup(const cmd::CmdStereoScan& command, const StereoScanOwnedResources& owned,
                                bool cleanup_camera_stream)
{
    if (owned.opened_projector) backend_.closeProjector(command.projector_role);
    if (owned.opened_window) backend_.closeWindow(command.window_role);
    if (!cleanup_camera_stream) return;
    if (owned.started_right_stream) backend_.stopStream(command.right_role);
    if (owned.started_left_stream) backend_.stopStream(command.left_role);
    if (owned.opened_right_camera) backend_.closeCamera(command.right_role);
    if (owned.opened_left_camera) backend_.closeCamera(command.left_role);
}

void StereoScanService::fail(const cmd::CmdStereoScan& command, const StereoScanOwnedResources& owned,
                             const std::string& stage, const std::string& code,
                             const std::string& message, bool cleanup_camera_stream)
{
    cleanup(command, owned, cleanup_camera_stream);
    push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", stage},
        {"error_code", code}, {"error_message", message}});
}

bool StereoScanService::validateCalibration(const std::filesystem::path& path,
                                            std::string& code, std::string& message) const
{
    if (!std::filesystem::exists(path))
    { code = "stereo_calibration_file_not_found"; message = "stereo calibration file not found: " + path.string(); return false; }
    try
    {
        cv::FileStorage storage(path.string(), cv::FileStorage::READ);
        cv::Mat k1, d1, k2, d2, r, t; int width = 0, height = 0;
        if (storage.isOpened()) { storage["K1"] >> k1; storage["D1"] >> d1; storage["K2"] >> k2;
            storage["D2"] >> d2; storage["R"] >> r; storage["T"] >> t;
            storage["image_width"] >> width; storage["image_height"] >> height; }
        if (!storage.isOpened() || k1.rows != 3 || k1.cols != 3 || k2.rows != 3 || k2.cols != 3 ||
            d1.empty() || d2.empty() || r.rows != 3 || r.cols != 3 || t.total() != 3 || width <= 0 || height <= 0)
        { code = "stereo_calibration_file_invalid"; message = "stereo calibration file is missing K/D, R/T, or image size"; return false; }
    }
    catch (const cv::Exception& error) { code = "stereo_calibration_file_invalid"; message = error.what(); return false; }
    return true;
}

void StereoScanService::push(std::string event, std::map<std::string, std::string> values)
{ events_.push(scan::ScanEvent{std::move(event), std::move(values)}); }

void StereoScanService::setInactive()
{
    std::lock_guard lock(mutex_); active_ = false; active_scan_id_.clear();
}

bool StereoScanService::isActive() const
{
    std::lock_guard lock(mutex_); return active_;
}

void StereoScanService::requestShutdown()
{
    std::string scan_id;
    {
        std::lock_guard lock(mutex_);
        scan_id = active_scan_id_;
        if (worker_.joinable()) worker_.request_stop();
    }
    if (!scan_id.empty()) backend_.requestStopScan(scan_id);
}

void StereoScanService::shutdown()
{
    requestShutdown();
    if (worker_.joinable()) worker_ = std::jthread{};
}
}
