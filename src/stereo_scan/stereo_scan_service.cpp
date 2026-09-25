#include "stereo_scan/stereo_scan_service.hpp"

#include "decode/decode_service.hpp"
#include "projector/projector_service.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "scan/scan_event.hpp"
#include "scan/scan_service.hpp"
#include "video/camera_service.hpp"
#include "window/window_service.hpp"

#include <chrono>
#include <filesystem>
#include <opencv2/core.hpp>
#include <thread>

namespace stereo_scan
{
namespace
{
common::CommandResult cameraFailure(const video::CameraResult& result)
{
    return common::failure(result.error ? result.error->code : "camera_open_failed",
                           result.error ? result.error->message : "camera open failed");
}
}

StereoScanService::StereoScanService(
    video::CameraService& cameras, win::WindowService& windows, projector::ProjectorService& projectors,
    scan::ScanService& scans, decode::DecodeService& decoder,
    reconstruction::ReconstructionService& reconstruction, scan::ScanEventSink& events,
    StreamOperation start_stream, StreamOperation stop_stream)
    : cameras_(cameras), windows_(windows), projectors_(projectors), scans_(scans), decoder_(decoder),
      reconstruction_(reconstruction), events_(events), start_stream_(std::move(start_stream)),
      stop_stream_(std::move(stop_stream)) {}

StereoScanService::~StereoScanService() { shutdown(); }

common::CommandResult StereoScanService::start(const cmd::CmdStereoScan& command)
{
    std::lock_guard lock(mutex_);
    if (active_) return common::failure("stereo_scan_already_running", "a stereo scan is already running");

    std::string code, message;
    if (!validateCalibration(command.calibration_file, code, message))
    {
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "reconstruction"},
             {"error_code", code}, {"error_message", message}});
        return common::failure(code, message);
    }
    try
    {
        std::filesystem::create_directories(command.output_dir / "scan");
        std::filesystem::create_directories(command.output_dir / "decode");
        if (command.ply_file.has_parent_path()) std::filesystem::create_directories(command.ply_file.parent_path());
    }
    catch (const std::exception& error) { return common::failure("directory_create_failed", error.what()); }

    const auto left_open = cameras_.openCamera(command.left_camera_id, command.left_role);
    if (!left_open.ok)
    {
        const auto result = cameraFailure(left_open);
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "camera"},
             {"error_code", result.error->code}, {"error_message", result.error->message}});
        return result;
    }
    const auto right_open = cameras_.openCamera(command.right_camera_id, command.right_role);
    if (!right_open.ok)
    {
        (void)cameras_.closeCamera(command.left_role);
        const auto result = cameraFailure(right_open);
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "camera"},
             {"error_code", result.error->code}, {"error_message", result.error->message}});
        return result;
    }
    const auto left_stream = start_stream_(command.left_role);
    if (!left_stream.ok)
    {
        (void)cameras_.closeCamera(command.right_role); (void)cameras_.closeCamera(command.left_role);
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "stream"},
             {"error_code", left_stream.error->code}, {"error_message", left_stream.error->message}});
        return left_stream;
    }
    const auto right_stream = start_stream_(command.right_role);
    if (!right_stream.ok)
    {
        (void)stop_stream_(command.left_role);
        (void)cameras_.closeCamera(command.right_role); (void)cameras_.closeCamera(command.left_role);
        push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", "stream"},
             {"error_code", right_stream.error->code}, {"error_message", right_stream.error->message}});
        return right_stream;
    }
    const auto left_url = left_stream.values.contains("url") ? left_stream.values.at("url") : std::string{};
    const auto right_url = right_stream.values.contains("url") ? right_stream.values.at("url") : std::string{};
    active_ = true;
    worker_ = std::jthread([this, command, left_url, right_url](std::stop_token token) {
        try { workerLoop(token, command, left_url, right_url); }
        catch (const std::exception& error) { fail(command, "scan", "internal_error", error.what(), true); }
        catch (...) { fail(command, "scan", "internal_error", "unhandled stereo scan error", true); }
    });
    push("stereo_scan_started", {{"scan_id", command.scan_id}, {"left_stream_url", left_url},
         {"right_stream_url", right_url}, {"output_dir", command.output_dir.string()}});
    return common::success({{"scan_id", command.scan_id}, {"left_stream_url", left_url},
        {"right_stream_url", right_url}, {"output_dir", command.output_dir.string()},
        {"ply_file", command.ply_file.string()}});
}

void StereoScanService::workerLoop(std::stop_token token, cmd::CmdStereoScan command,
                                   std::string left_url, std::string right_url)
{
    const auto window = windows_.openWindow({command.window_role, "Projector",
        command.display_width.value_or(command.code_width), command.display_height.value_or(command.code_height),
        command.monitor_index, true, command.post_open_key, command.post_open_action});
    if (!window.ok) { fail(command, "window", window.error->code, window.error->message, true); return; }
    const auto opened = projectors_.openProjector({command.projector_role, command.window_role,
                                                    command.code_width, command.code_height});
    if (!opened.ok) { fail(command, "projector", opened.error->code, opened.error->message, true); return; }
    const auto surface = projectors_.configureSurface({command.projector_role, command.monitor_index,
        command.display_width.value_or(command.code_width), command.display_height.value_or(command.code_height),
        command.x, command.y, command.placement == "custom" ? projector::ProjectorPlacement::custom
                                                                  : projector::ProjectorPlacement::center});
    if (!surface.ok) { fail(command, "projector", surface.error->code, surface.error->message, true); return; }
    const auto patterns = projectors_.generatePatterns(command.projector_role);
    if (!patterns.ok) { fail(command, "projector", patterns.error->code, patterns.error->message, true); return; }

    push("stereo_scan_scanning", {{"scan_id", command.scan_id}});
    scan::ScanStartConfig scan_config;
    scan_config.scan_id = command.scan_id;
    scan_config.projector_role = command.projector_role;
    scan_config.left_role = command.left_role;
    scan_config.right_role = command.right_role;
    scan_config.output_dir = command.output_dir / "scan";
    scan_config.sync_mode = command.sync_mode == "photodiode" ? scan::ScanSyncMode::photodiode
                                                               : scan::ScanSyncMode::delay;
    scan_config.delay_ms = command.delay_ms;
    scan_config.photodiode_device = command.photodiode_device;
    scan_config.photodiode_baud = command.photodiode_baud;
    scan_config.sync_timeout_ms = command.sync_timeout_ms;
    scan_config.sync_guard_ms = command.guard_ms;
    const auto started = scans_.startScan(scan_config);
    if (!started.ok) { fail(command, "scan", started.error->code, started.error->message, true); return; }
    scan::ScanResult status;
    while (!token.stop_requested())
    {
        status = scans_.scanStatus(command.scan_id);
        if (!status.ok || status.status == scan::ScanState::completed || status.status == scan::ScanState::failed ||
            status.status == scan::ScanState::stopped) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (token.stop_requested()) { (void)scans_.stopScan(command.scan_id); fail(command, "scan", "scan_stopped", "stereo scan stopped", true); return; }
    if (!status.ok || status.status != scan::ScanState::completed)
    {
        fail(command, "scan", status.error ? status.error->code : "scan_failed",
             status.error ? status.error->message : "scan did not complete", true); return;
    }

    push("stereo_scan_decoding", {{"scan_id", command.scan_id}});
    decode::DecodePatternsConfig decode_config;
    decode_config.input_dir = command.output_dir / "scan";
    decode_config.output_dir = command.output_dir / "decode";
    decode_config.threshold = command.decode_threshold;
    const auto decoded = decoder_.decodePatterns(decode_config);
    if (!decoded.ok) { fail(command, "decode", decoded.error->code, decoded.error->message, true); return; }

    push("stereo_scan_reconstructing", {{"scan_id", command.scan_id}});
    const auto reconstruction = reconstruction_.reconstruct({
        {command.output_dir / "decode", command.calibration_file, {command.max_epipolar_error_px, {}, {}}},
        command.ply_file, false});
    if (!reconstruction.ok)
    {
        fail(command, "reconstruction", reconstruction.error ? reconstruction.error->code : "triangulation_failed",
             reconstruction.error ? reconstruction.error->message : "reconstruction failed", true); return;
    }
    push("stereo_scan_completed", {{"scan_id", command.scan_id}, {"left_stream_url", left_url},
        {"right_stream_url", right_url}, {"scan_dir", (command.output_dir / "scan").string()},
        {"decode_dir", (command.output_dir / "decode").string()}, {"ply_file", reconstruction.output_file.string()},
        {"point_count", std::to_string(reconstruction.point_count)}});
    std::lock_guard lock(mutex_); active_ = false;
}

void StereoScanService::fail(const cmd::CmdStereoScan& command, const std::string& stage,
                             const std::string& code, const std::string& message, bool cleanup)
{
    if (cleanup)
    {
        (void)projectors_.closeProjector(command.projector_role);
        (void)windows_.closeWindow(command.window_role);
        (void)stop_stream_(command.right_role); (void)stop_stream_(command.left_role);
        (void)cameras_.closeCamera(command.right_role); (void)cameras_.closeCamera(command.left_role);
    }
    push("stereo_scan_failed", {{"scan_id", command.scan_id}, {"stage", stage},
        {"error_code", code}, {"error_message", message}});
    std::lock_guard lock(mutex_); active_ = false;
}

bool StereoScanService::validateCalibration(const std::filesystem::path& path,
                                            std::string& code, std::string& message) const
{
    if (!std::filesystem::exists(path)) { code = "stereo_calibration_file_not_found"; message = "stereo calibration file not found: " + path.string(); return false; }
    try
    {
        cv::FileStorage storage(path.string(), cv::FileStorage::READ);
        cv::Mat k1, d1, k2, d2, r, t;
        int width = 0, height = 0;
        if (storage.isOpened()) { storage["K1"] >> k1; storage["D1"] >> d1; storage["K2"] >> k2; storage["D2"] >> d2; storage["R"] >> r; storage["T"] >> t; storage["image_width"] >> width; storage["image_height"] >> height; }
        if (!storage.isOpened() || k1.rows != 3 || k1.cols != 3 || k2.rows != 3 || k2.cols != 3 ||
            d1.empty() || d2.empty() || r.rows != 3 || r.cols != 3 || t.total() != 3 || width <= 0 || height <= 0)
        { code = "stereo_calibration_file_invalid"; message = "stereo calibration file is missing K/D, R/T, or image size"; return false; }
    }
    catch (const cv::Exception& error) { code = "stereo_calibration_file_invalid"; message = error.what(); return false; }
    return true;
}

void StereoScanService::push(std::string event, std::map<std::string, std::string> values)
{ events_.push(scan::ScanEvent{std::move(event), std::move(values)}); }

void StereoScanService::shutdown()
{
    if (worker_.joinable()) { worker_.request_stop(); worker_ = std::jthread{}; }
}
}
