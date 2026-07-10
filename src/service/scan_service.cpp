#include "service/scan_service.hpp"

#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"
#include "service/camera_service.hpp"
#include "service/projector_service.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <utility>

namespace service::scan
{
namespace
{

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (const auto ch : value)
    {
        if (ch == '\\' || ch == '"')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

std::string nowCompact()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return stream.str();
}

std::string nowIsoLike()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return stream.str();
}

std::string captureCode(const capture::CaptureStereoResult& result)
{
    if (!result.error)
    {
        return "capture_failed";
    }
    switch (result.error->code)
    {
    case capture::CaptureErrorCode::CameraNotFound:
    case capture::CaptureErrorCode::CameraNotOpen:
        return "camera_not_open";
    case capture::CaptureErrorCode::InvalidOutputPath:
        return "invalid_output_path";
    case capture::CaptureErrorCode::DirectoryCreateFailed:
        return "directory_create_failed";
    case capture::CaptureErrorCode::EmptyFrame:
        return "empty_frame";
    case capture::CaptureErrorCode::FileWriteFailed:
        return "file_write_failed";
    case capture::CaptureErrorCode::InternalError:
        return "capture_failed";
    }
    return "capture_failed";
}

std::string captureMessage(const capture::CaptureStereoResult& result)
{
    return result.error ? result.error->message : std::string{"failed to capture stereo frame"};
}

} // namespace

ScanService::ScanService(service::projector::ProjectorService& projector_service, capture::CaptureService& capture_service,
                         service::camera::CameraService& camera_service, ScanEventSink& event_sink)
    : projector_service_(projector_service), capture_service_(capture_service), camera_service_(camera_service),
      event_sink_(event_sink)
{
}

ScanService::~ScanService()
{
    shutdown();
}

ScanResult ScanService::startScan(const ScanStartConfig& config)
{
    if (config.projector_role.empty() || config.left_role.empty() || config.right_role.empty() || config.settle_ms < 0)
    {
        return ScanResult::failure(config.scan_id.value_or(std::string{}), "invalid_scan_config", "invalid scan configuration");
    }
    if (config.output_dir.empty())
    {
        return ScanResult::failure(config.scan_id.value_or(std::string{}), "invalid_output_path", "output_dir is empty");
    }
    if (config.scan_id && config.scan_id->empty())
    {
        return ScanResult::failure({}, "invalid_scan_config", "scan_id must not be empty");
    }

    const auto scan_id = config.scan_id.value_or(generateScanId());
    {
        std::lock_guard lock(mutex_);
        if (state_ == ScanState::running || state_ == ScanState::stopping)
        {
            return ScanResult::failure(active_scan_id_, "scan_already_running", "scan is already running");
        }
    }

    const auto snapshot = projector_service_.scanSnapshot(config.projector_role);
    if (!snapshot)
    {
        return ScanResult::failure(scan_id, "projector_not_open", "projector role is not open: " + config.projector_role);
    }
    if (snapshot->patterns_dirty || snapshot->pattern_count <= 0)
    {
        return ScanResult::failure(scan_id, "pattern_not_generated", "patterns are not generated");
    }

    const auto left_id = camera_service_.resolveCameraId(config.left_role);
    const auto right_id = camera_service_.resolveCameraId(config.right_role);
    if (!left_id || !right_id)
    {
        return ScanResult::failure(scan_id, "camera_not_open", "left or right camera role is not open");
    }

    try
    {
        std::filesystem::create_directories(config.output_dir / "left");
        std::filesystem::create_directories(config.output_dir / "right");
    }
    catch (const std::exception& error)
    {
        return ScanResult::failure(scan_id, "directory_create_failed", error.what());
    }

    std::string metadata_error;
    if (!writeMetadata(config, scan_id, snapshot->pattern_count, snapshot->surface, metadata_error))
    {
        return ScanResult::failure(scan_id, "scan_start_failed", metadata_error);
    }

    auto worker_config = config;
    worker_config.scan_id = scan_id;
    {
        std::lock_guard lock(mutex_);
        active_scan_id_ = scan_id;
        active_config_ = worker_config;
        state_ = ScanState::running;
        pattern_count_ = snapshot->pattern_count;
        captured_count_ = 0;
        current_index_ = -1;
        last_error_code_.clear();
        last_error_message_.clear();
    }

    if (worker_.joinable())
    {
        worker_.request_stop();
        worker_ = std::jthread{};
    }
    worker_ = std::jthread([this, worker_config, scan_id, pattern_count = snapshot->pattern_count](std::stop_token token)
                           { workerLoop(token, worker_config, scan_id, pattern_count); });

    auto result = scanStatus(scan_id);
    result.ok = true;
    return result;
}

ScanResult ScanService::scanStatus(std::optional<std::string> scan_id) const
{
    std::lock_guard lock(mutex_);
    if (active_scan_id_.empty())
    {
        if (scan_id)
        {
            return ScanResult::failure(*scan_id, "scan_not_found", "scan not found");
        }
        return ScanResult::success({}, ScanState::idle);
    }
    if (scan_id && *scan_id != active_scan_id_)
    {
        return ScanResult::failure(*scan_id, "scan_not_found", "scan not found");
    }
    return snapshotLocked();
}

ScanResult ScanService::stopScan(std::optional<std::string> scan_id)
{
    {
        std::lock_guard lock(mutex_);
        if (active_scan_id_.empty() || (scan_id && *scan_id != active_scan_id_))
        {
            return ScanResult::failure(scan_id.value_or(std::string{}), "scan_not_found", "scan not found");
        }
        if (state_ == ScanState::running)
        {
            state_ = ScanState::stopping;
        }
    }

    if (worker_.joinable())
    {
        worker_.request_stop();
    }

    const auto result = scanStatus(scan_id);
    pushEvent("scan_stopping", {{"scan_id", result.scan_id}});
    return result;
}

void ScanService::shutdown()
{
    {
        std::lock_guard lock(mutex_);
        if (state_ == ScanState::running)
        {
            state_ = ScanState::stopping;
        }
    }
    if (worker_.joinable())
    {
        worker_.request_stop();
        worker_ = std::jthread{};
    }
}

void ScanService::workerLoop(std::stop_token stop_token, ScanStartConfig config, std::string scan_id, int pattern_count)
{
    pushEvent("scan_started", {{"scan_id", scan_id},
                                {"projector_role", config.projector_role},
                                {"left_role", config.left_role},
                                {"right_role", config.right_role},
                                {"pattern_count", std::to_string(pattern_count)},
                                {"output_dir", config.output_dir.string()}});

    const auto left_id = camera_service_.resolveCameraId(config.left_role);
    const auto right_id = camera_service_.resolveCameraId(config.right_role);
    if (!left_id || !right_id)
    {
        std::lock_guard lock(mutex_);
        state_ = ScanState::failed;
        last_error_code_ = "camera_not_open";
        last_error_message_ = "left or right camera role is not open";
        pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                   {"error_message", last_error_message_}, {"captured_count", std::to_string(captured_count_)},
                                   {"current_index", std::to_string(current_index_)}});
        return;
    }

    for (int index = 0; index < pattern_count; ++index)
    {
        if (stop_token.stop_requested())
        {
            std::lock_guard lock(mutex_);
            state_ = ScanState::stopped;
            pushEvent("scan_stopped", {{"scan_id", scan_id}, {"captured_count", std::to_string(captured_count_)},
                                        {"current_index", std::to_string(current_index_)}});
            return;
        }
        {
            std::lock_guard lock(mutex_);
            current_index_ = index;
        }

        const auto show_result = projector_service_.showPattern(config.projector_role, index);
        if (!show_result.ok)
        {
            std::lock_guard lock(mutex_);
            state_ = ScanState::failed;
            last_error_code_ = show_result.error ? show_result.error->code : std::string{"pattern_show_failed"};
            last_error_message_ = show_result.error ? show_result.error->message : std::string{"failed to show pattern"};
            pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                       {"error_message", last_error_message_}, {"captured_count", std::to_string(captured_count_)},
                                       {"current_index", std::to_string(current_index_)}});
            return;
        }

        if (config.settle_ms > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.settle_ms));
        }

        const auto left_path = config.output_dir / "left" / patternFileName(index);
        const auto right_path = config.output_dir / "right" / patternFileName(index);
        capture::CaptureStereoResult capture_result;
        {
            std::lock_guard capture_lock(scan_capture_mutex_);
            capture_result = capture_service_.captureStereo(*left_id, *right_id, left_path, right_path);
        }
        if (!capture_result.ok)
        {
            std::lock_guard lock(mutex_);
            state_ = ScanState::failed;
            last_error_code_ = captureCode(capture_result);
            last_error_message_ = captureMessage(capture_result);
            pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                       {"error_message", last_error_message_}, {"captured_count", std::to_string(captured_count_)},
                                       {"current_index", std::to_string(current_index_)}});
            return;
        }

        int captured = 0;
        {
            std::lock_guard lock(mutex_);
            captured_count_ += 1;
            captured = captured_count_;
        }
        pushEvent("scan_frame_captured", {{"scan_id", scan_id},
                                           {"pattern_index", std::to_string(index)},
                                           {"captured_count", std::to_string(captured)},
                                           {"pattern_count", std::to_string(pattern_count)},
                                           {"left_path", left_path.string()},
                                           {"right_path", right_path.string()}});
    }

    {
        std::lock_guard lock(mutex_);
        state_ = ScanState::completed;
        current_index_ = pattern_count > 0 ? pattern_count - 1 : -1;
    }
    pushEvent("scan_completed", {{"scan_id", scan_id},
                                  {"captured_count", std::to_string(pattern_count)},
                                  {"pattern_count", std::to_string(pattern_count)},
                                  {"output_dir", config.output_dir.string()}});
}

ScanResult ScanService::snapshotLocked() const
{
    auto result = ScanResult::success(active_scan_id_, state_);
    result.projector_role = active_config_.projector_role;
    result.left_role = active_config_.left_role;
    result.right_role = active_config_.right_role;
    result.output_dir = active_config_.output_dir.string();
    result.pattern_count = pattern_count_;
    result.captured_count = captured_count_;
    result.current_index = current_index_;
    if (state_ == ScanState::failed && !last_error_code_.empty())
    {
        result.error = ScanError{last_error_code_, last_error_message_};
    }
    return result;
}

void ScanService::pushEvent(std::string event, std::map<std::string, std::string> values)
{
    event_sink_.push(ScanEvent{std::move(event), std::move(values)});
}

bool ScanService::writeMetadata(const ScanStartConfig& config, const std::string& scan_id, int pattern_count,
                                const service::projector::ProjectorSurface& surface, std::string& error_message) const
{
    try
    {
        std::ofstream output(config.output_dir / "metadata.json");
        if (!output)
        {
            error_message = "failed to open metadata.json";
            return false;
        }
        output << "{\n"
               << "  \"scan_id\": \"" << jsonEscape(scan_id) << "\",\n"
               << "  \"created_at\": \"" << jsonEscape(nowIsoLike()) << "\",\n"
               << "  \"version\": \"0.1.0\",\n"
               << "  \"projector_role\": \"" << jsonEscape(config.projector_role) << "\",\n"
               << "  \"left_role\": \"" << jsonEscape(config.left_role) << "\",\n"
               << "  \"right_role\": \"" << jsonEscape(config.right_role) << "\",\n"
               << "  \"pattern_count\": " << pattern_count << ",\n"
               << "  \"settle_ms\": " << config.settle_ms << ",\n"
               << "  \"output_dir\": \"" << jsonEscape(config.output_dir.string()) << "\",\n"
               << "  \"surface\": {\n"
               << "    \"monitor_index\": " << surface.monitor_index << ",\n"
               << "    \"monitor_width\": " << surface.monitor_width << ",\n"
               << "    \"monitor_height\": " << surface.monitor_height << ",\n"
               << "    \"surface_width\": " << surface.surface_width << ",\n"
               << "    \"surface_height\": " << surface.surface_height << ",\n"
               << "    \"pattern_width\": " << surface.pattern_width << ",\n"
               << "    \"pattern_height\": " << surface.pattern_height << ",\n"
               << "    \"pattern_x\": " << surface.pattern_x << ",\n"
               << "    \"pattern_y\": " << surface.pattern_y << ",\n"
               << "    \"clamped\": " << (surface.clamped ? "true" : "false") << "\n"
               << "  }\n"
               << "}\n";
        return true;
    }
    catch (const std::exception& error)
    {
        error_message = error.what();
        return false;
    }
}

std::string ScanService::generateScanId()
{
    return "scan_" + nowCompact();
}

std::string ScanService::patternFileName(int index)
{
    std::ostringstream stream;
    stream << "pattern_" << std::setfill('0') << std::setw(3) << index << ".png";
    return stream.str();
}

} // namespace service::scan
