#include "scan/scan_service.hpp"
#include "scan/serial_photodiode_transport.hpp"

#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"
#include "video/camera_service.hpp"
#include "projector/projector_service.hpp"
#include "structured_light/pattern_sync.hpp"

#include <algorithm>
#include <chrono>
#include <opencv2/core.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <utility>

namespace scan
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

cv::Mat medianFrame(const std::vector<video::FrameSample>& frames)
{
    if (frames.size() < 3) return {};
    const auto& a = frames[frames.size() - 3].image;
    const auto& b = frames[frames.size() - 2].image;
    const auto& c = frames[frames.size() - 1].image;
    if (a.empty() || a.size() != b.size() || a.size() != c.size() || a.type() != b.type() || a.type() != c.type())
        return {};
    cv::Mat minimum_ab, maximum_ab, minimum_abc, maximum_abc, sum;
    cv::min(a, b, minimum_ab);
    cv::max(a, b, maximum_ab);
    cv::min(minimum_ab, c, minimum_abc);
    cv::max(maximum_ab, c, maximum_abc);
    cv::add(a, b, sum, cv::noArray(), CV_16U);
    cv::add(sum, c, sum, cv::noArray(), CV_16U);
    cv::Mat extremes;
    cv::add(minimum_abc, maximum_abc, extremes, cv::noArray(), CV_16U);
    cv::subtract(sum, extremes, sum);
    cv::Mat result;
    sum.convertTo(result, a.type());
    return result;
}

std::int64_t timestampNs(std::chrono::steady_clock::time_point value)
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(value.time_since_epoch()).count();
}

} // namespace

ScanService::ScanService(projector::ProjectorService& projector_service,
                         capture::CaptureService& capture_service, video::CameraService& camera_service,
                         ScanEventSink& event_sink, PhotodiodeTransportFactory photodiode_factory)
    : projector_service_(projector_service), capture_service_(capture_service), camera_service_(camera_service),
      event_sink_(event_sink), photodiode_factory_(std::move(photodiode_factory))
{
    if (!photodiode_factory_)
    {
        photodiode_factory_ = [](const std::string& device, int baud) {
            return std::make_unique<SerialPhotodiodeTransport>(device, baud);
        };
    }
}

ScanService::~ScanService()
{
    shutdown();
}

ScanResult ScanService::startScan(const ScanStartConfig& config)
{
    if (config.projector_role.empty() || config.left_role.empty() || config.photodiode_device.empty() ||
        config.photodiode_baud <= 0 || config.sync_timeout_ms <= 0 || config.sync_guard_ms < 0 ||
        config.max_patterns < 0)
    {
        return ScanResult::failure(config.scan_id.value_or(std::string{}), "invalid_scan_config",
                                   "invalid scan configuration");
    }
    if (config.output_dir.empty())
    {
        return ScanResult::failure(config.scan_id.value_or(std::string{}), "invalid_output_path",
                                   "output_dir is empty");
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
        return ScanResult::failure(scan_id, "projector_not_open",
                                   "projector role is not open: " + config.projector_role);
    }
    if (snapshot->patterns_dirty || snapshot->pattern_count <= 0)
    {
        return ScanResult::failure(scan_id, "pattern_not_generated", "patterns are not generated");
    }
    if (!projector::canPlacePhotodiodeMarker(snapshot->surface))
    {
        return ScanResult::failure(scan_id, "photodiode_marker_margin_unavailable",
                                   "photodiode synchronization requires a 32x32 projector margin outside the active pattern");
    }

    const auto left_id = camera_service_.resolveCameraId(config.left_role);
    const auto right_id = config.right_role.empty() ? std::optional<video::CameraId>{} : camera_service_.resolveCameraId(config.right_role);
    if (!left_id || (!config.right_role.empty() && !right_id))
    {
        return ScanResult::failure(scan_id, "camera_not_open", "left or right camera role is not open");
    }

    try
    {
        std::filesystem::create_directories(config.output_dir / "left");
        if (!config.right_role.empty()) std::filesystem::create_directories(config.output_dir / "right");
    }
    catch (const std::exception& error)
    {
        return ScanResult::failure(scan_id, "directory_create_failed", error.what());
    }

    std::string metadata_error;
    const int scan_pattern_count = config.max_patterns > 0
                                       ? std::min(config.max_patterns, snapshot->pattern_count)
                                       : snapshot->pattern_count;
    if (!writeMetadata(config, scan_id, scan_pattern_count, snapshot->code_width, snapshot->code_height,
                       snapshot->surface, metadata_error))
    {
        return ScanResult::failure(scan_id, "scan_start_failed", metadata_error);
    }

    auto worker_config = config;
    worker_config.scan_id = scan_id;
    {
        std::lock_guard lock(mutex_);
        active_scan_id_ = scan_id;
        active_config_ = worker_config;
        active_window_role_ = snapshot->window_role;
        state_ = ScanState::running;
        pattern_count_ = scan_pattern_count;
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
    worker_ = std::jthread([this, worker_config, scan_id, pattern_count = scan_pattern_count](
                               std::stop_token token) { workerLoop(token, worker_config, scan_id, pattern_count); });

    auto result = scanStatus(scan_id);
    result.ok = true;
    return result;
}

SyncDelayResult ScanService::measureSyncDelay(const SyncDelayConfig& config)
{
    std::unique_lock measurement_lock(sync_delay_mutex_, std::try_to_lock);
    if (!measurement_lock.owns_lock())
        return {false, "sync_delay_already_running", "sync delay measurement is already running"};
    if (config.projector_role.empty() || config.camera_role.empty() || config.photodiode_device.empty() ||
        config.photodiode_baud <= 0 || config.transitions <= 0 || config.sync_timeout_ms <= 0 ||
        config.safety_margin_ms < 0.0 || config.minimum_contrast <= 0.0 ||
        config.required_ratio <= 0.0 || config.required_ratio > 1.0)
        return {false, "invalid_sync_delay_config", "invalid sync delay measurement configuration"};
    if (isScanActive())
        return {false, "scan_resource_busy", "sync delay measurement conflicts with active scan"};

    const auto snapshot = projector_service_.scanSnapshot(config.projector_role);
    if (!snapshot) return {false, "projector_not_open", "projector role is not open: " + config.projector_role};
    if (snapshot->patterns_dirty || snapshot->pattern_count < 2)
        return {false, "pattern_not_generated", "BLACK/WHITE reference patterns are not generated"};
    if (!projector::canPlacePhotodiodeMarker(snapshot->surface))
        return {false, "photodiode_marker_margin_unavailable", "Photodiode marker cannot be placed"};
    const auto camera_id = camera_service_.resolveCameraId(config.camera_role);
    if (!camera_id) return {false, "camera_not_open", "camera role is not open: " + config.camera_role};
    const auto initial_frame = camera_service_.latestFrame(*camera_id);
    if (!initial_frame) return {false, "camera_frame_timeout", "camera has not produced a frame"};

    const auto timeout = std::chrono::milliseconds(config.sync_timeout_ms);
    auto show = [&](int index) -> std::optional<SyncDelayResult> {
        const auto result = projector_service_.showPattern(config.projector_role, index,
                                                            projector::PhotodiodeMarkerMode::sync);
        if (!result.ok)
            return SyncDelayResult{false,
                                   result.error ? result.error->code : "pattern_show_failed",
                                   result.error ? result.error->message : "failed to show pattern"};
        return std::nullopt;
    };
    auto stableBaseline = [&](int index, const char* state) -> std::optional<cv::Mat> {
        const auto latest = camera_service_.latestFrame(*camera_id);
        if (!latest) return std::nullopt;
        if (const auto failure = show(index)) return std::nullopt;
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::vector<video::FrameSample> frames;
        while (std::chrono::steady_clock::now() < deadline)
        {
            frames = camera_service_.frameSamplesAfter(*camera_id, latest->sequence);
            if (frames.size() >= 5) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (frames.size() < 5) return std::nullopt;
        pushEvent("sync_delay_baseline", {{"state", state}, {"frame_sequence", std::to_string(frames.back().sequence)}});
        return medianFrame(frames);
    };

    pushEvent("sync_delay_baseline_started", {});
    const auto black = stableBaseline(0, "black");
    if (!black) return {false, "camera_frame_timeout", "BLACK baseline frames did not arrive"};
    const auto white = stableBaseline(1, "white");
    if (!white) return {false, "camera_frame_timeout", "WHITE baseline frames did not arrive"};
    const auto model = buildMeasurementModel(*black, *white, config.minimum_contrast);
    if (!model)
        return {false, "sync_delay_insufficient_contrast",
                "BLACK/WHITE baselines do not contain enough contrasting pixels"};
    pushEvent("sync_delay_mask_ready", {{"measurement_pixel_count", std::to_string(model->pixel_count)}});

    std::unique_ptr<structured_light::sync::PhotodiodeTransport> transport;
    try { transport = photodiode_factory_(config.photodiode_device, config.photodiode_baud); }
    catch (const PhotodiodeTransportError& error)
    { return {false, error.code(), error.what()}; }
    catch (const std::exception& error)
    { return {false, "photodiode_open_failed", error.what()}; }
    structured_light::sync::PhotodiodeSyncSource source(*transport);

    pushEvent("sync_delay_prearm_started", {});
    const auto white_after = std::chrono::steady_clock::now();
    if (const auto failure = show(1)) return *failure;
    (void)source.waitForTransition(structured_light::sync::MarkerState::white, white_after,
                                   std::chrono::milliseconds(std::min(200, config.sync_timeout_ms)));
    const auto black_after = std::chrono::steady_clock::now();
    if (const auto failure = show(0)) return *failure;
    const auto ready = source.waitForTransition(structured_light::sync::MarkerState::black, black_after, timeout);
    if (!ready) return {false, "photodiode_timeout", "Photodiode BLACK pre-arm event was not received"};
    pushEvent("sync_delay_prearm_ready", {{"state", "black"}});

    SyncDelayResult result;
    result.measurement_pixel_count = model->pixel_count;
    const auto states = measurementStateSequence(config.transitions);
    for (std::size_t index = 0; index < states.size(); ++index)
    {
        const auto expected = states[index];
        const int pattern_index = expected == structured_light::sync::MarkerState::white ? 1 : 0;
        const auto shown_after = std::chrono::steady_clock::now();
        if (const auto failure = show(pattern_index)) return *failure;
        std::optional<structured_light::sync::SyncEvent> event;
        try { event = source.waitForTransition(expected, shown_after, timeout); }
        catch (const PhotodiodeTransportError& error) { return {false, error.code(), error.what()}; }
        if (!event) return {false, "photodiode_timeout", "Photodiode transition event was not received"};

        std::uint64_t cursor = 0;
        if (const auto at_event = camera_service_.firstFrameAtOrAfter(*camera_id, event->timestamp))
            cursor = at_event->sequence > 0 ? at_event->sequence - 1 : 0;
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::optional<video::FrameSample> matched_frame;
        double matched_ratio = 0.0;
        while (std::chrono::steady_clock::now() < deadline && !matched_frame)
        {
            const auto frames = camera_service_.frameSamplesAfter(*camera_id, cursor);
            for (const auto& frame : frames)
            {
                cursor = frame.sequence;
                if (frame.timestamp < event->timestamp) continue;
                double ratio = 0.0;
                if (frameMatches(*model, frame.image, expected, config.required_ratio, &ratio))
                {
                    matched_frame = frame;
                    matched_ratio = ratio;
                    break;
                }
            }
            if (!matched_frame) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!matched_frame)
            return {false, "camera_transition_timeout", "Camera did not observe the expected projector state"};
        const double delay_ms = std::chrono::duration<double, std::milli>(
                                    matched_frame->timestamp - event->timestamp).count();
        result.measurements.push_back({static_cast<int>(index + 1), expected, event->timestamp,
                                       matched_frame->timestamp, delay_ms, matched_frame->sequence,
                                       matched_ratio});
        pushEvent("sync_delay_transition",
                  {{"sequence", std::to_string(index + 1)}, {"total", std::to_string(states.size())},
                   {"state", structured_light::sync::toString(expected)}, {"delay_ms", std::to_string(delay_ms)},
                   {"frame_sequence", std::to_string(matched_frame->sequence)},
                   {"matched_ratio", std::to_string(matched_ratio)}});
    }

    std::vector<double> delays;
    delays.reserve(result.measurements.size());
    for (const auto& measurement : result.measurements) delays.push_back(measurement.delay_ms);
    result.statistics = calculateSyncDelayStatistics(delays, config.safety_margin_ms);
    result.ok = true;
    result.csv_path = config.output_csv.string();
    try
    {
        if (config.output_csv.has_parent_path()) std::filesystem::create_directories(config.output_csv.parent_path());
        std::ofstream csv(config.output_csv);
        if (!csv) throw std::runtime_error("failed to open CSV output");
        csv << "sequence,state,photodiode_ns,camera_ns,delay_ms,frame_sequence,matched_ratio\n";
        csv << std::fixed << std::setprecision(6);
        for (const auto& item : result.measurements)
            csv << item.sequence << ',' << structured_light::sync::toString(item.state) << ','
                << timestampNs(item.photodiode_timestamp) << ',' << timestampNs(item.camera_timestamp) << ','
                << item.delay_ms << ',' << item.frame_sequence << ',' << item.matched_ratio << '\n';
        if (!csv) throw std::runtime_error("failed to write CSV output");
    }
    catch (const std::exception& error) { result.csv_warning = error.what(); }
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

bool ScanService::isScanActive() const
{
    std::lock_guard lock(mutex_);
    return state_ == ScanState::running || state_ == ScanState::stopping;
}

bool ScanService::isProjectorRoleBusy(const std::string& projector_role) const
{
    std::lock_guard lock(mutex_);
    if (state_ != ScanState::running && state_ != ScanState::stopping)
    {
        return false;
    }
    return active_config_.projector_role == projector_role;
}

bool ScanService::isCameraRoleBusy(const std::string& camera_role) const
{
    std::lock_guard lock(mutex_);
    if (state_ != ScanState::running && state_ != ScanState::stopping)
    {
        return false;
    }
    return active_config_.left_role == camera_role || active_config_.right_role == camera_role;
}

bool ScanService::isWindowRoleBusy(const std::string& window_role) const
{
    std::lock_guard lock(mutex_);
    if (state_ != ScanState::running && state_ != ScanState::stopping)
    {
        return false;
    }
    return active_window_role_ == window_role;
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
    const auto right_id = config.right_role.empty() ? std::optional<video::CameraId>{} : camera_service_.resolveCameraId(config.right_role);
    if (!left_id || (!config.right_role.empty() && !right_id))
    {
        int captured = 0;
        int current = -1;
        std::string error_code;
        std::string error_message;
        {
            std::lock_guard lock(mutex_);
            state_ = ScanState::failed;
            last_error_code_ = "camera_not_open";
            last_error_message_ = "left or right camera role is not open";
            captured = captured_count_;
            current = current_index_;
            error_code = last_error_code_;
            error_message = last_error_message_;
        }
        pushEvent("scan_failed", {{"scan_id", scan_id},
                                  {"error_code", error_code},
                                  {"error_message", error_message},
                                  {"captured_count", std::to_string(captured)},
                                  {"current_index", std::to_string(current)}});
        return;
    }

    std::unique_ptr<structured_light::sync::PhotodiodeTransport> photodiode_transport;
    try
    {
        photodiode_transport = photodiode_factory_(config.photodiode_device, config.photodiode_baud);
    }
    catch (const PhotodiodeTransportError& error)
    {
        std::lock_guard lock(mutex_);
        state_ = ScanState::failed;
        last_error_code_ = error.code();
        last_error_message_ = error.what();
        pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                  {"error_message", last_error_message_}});
        return;
    }
    structured_light::sync::PhotodiodeSyncSource photodiode_source(*photodiode_transport);

    // Locatorは実機配置専用。production scanは常に従来の32x32 black/white契約を使う。
    const auto sync_mode_result = projector_service_.setPhotodiodeMarkerMode(
        config.projector_role, projector::PhotodiodeMarkerMode::sync);
    if (!sync_mode_result.ok)
    {
        std::lock_guard lock(mutex_);
        state_ = ScanState::failed;
        last_error_code_ = "pattern_show_failed";
        last_error_message_ = "failed to select Photodiode sync marker mode";
        pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                  {"error_message", last_error_message_}});
        return;
    }

    // MCUは状態変化時だけeventを送る。scan開始前にmarkerをwhiteへ確立し、
    // pattern 0 (black)が必ずtransitionになるようpre-armする。
    try
    {
        const auto black_result = projector_service_.showPattern(config.projector_role, 0);
        if (!black_result.ok)
        {
            throw PhotodiodeTransportError("pattern_show_failed", "failed to show black pre-arm pattern");
        }
        const auto black_shown_at = std::chrono::steady_clock::now();
        // 現在すでにblackならMCUはeventを送らないため、black確認timeoutは許容する。
        // 続くwhite eventだけを必須にし、pattern 0のblack transitionを保証する。
        (void)photodiode_source.waitForTransition(
            structured_light::sync::MarkerState::black, black_shown_at,
            std::chrono::milliseconds(std::min(config.sync_timeout_ms, 200)));

        const auto white_result = projector_service_.showPattern(config.projector_role, 1);
        if (!white_result.ok)
        {
            throw PhotodiodeTransportError("pattern_show_failed", "failed to show white pre-arm pattern");
        }
        const auto white_shown_at = std::chrono::steady_clock::now();
        const auto white_event = photodiode_source.waitForTransition(
            structured_light::sync::MarkerState::white, white_shown_at,
            std::chrono::milliseconds(config.sync_timeout_ms));
        if (!white_event)
        {
            throw PhotodiodeTransportError("photodiode_timeout", "Photodiode pre-arm white eventを受信できませんでした");
        }
    }
    catch (const PhotodiodeTransportError& error)
    {
        std::lock_guard lock(mutex_);
        state_ = ScanState::failed;
        last_error_code_ = error.code();
        last_error_message_ = error.what();
        pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                  {"error_message", last_error_message_}});
        return;
    }

    for (int index = 0; index < pattern_count; ++index)
    {
        if (stop_token.stop_requested())
        {
            int captured = 0;
            int current = -1;
            {
                std::lock_guard lock(mutex_);
                state_ = ScanState::stopped;
                captured = captured_count_;
                current = current_index_;
            }
            pushEvent("scan_stopped", {{"scan_id", scan_id},
                                       {"captured_count", std::to_string(captured)},
                                       {"current_index", std::to_string(current)}});
            return;
        }
        {
            std::lock_guard lock(mutex_);
            current_index_ = index;
        }

        const auto show_result = projector_service_.showPattern(config.projector_role, index);
        if (!show_result.ok)
        {
            int captured = 0;
            int current = -1;
            std::string error_code;
            std::string error_message;
            {
                std::lock_guard lock(mutex_);
                state_ = ScanState::failed;
                last_error_code_ = show_result.error ? show_result.error->code : std::string{"pattern_show_failed"};
                last_error_message_ =
                    show_result.error ? show_result.error->message : std::string{"failed to show pattern"};
                captured = captured_count_;
                current = current_index_;
                error_code = last_error_code_;
                error_message = last_error_message_;
            }
            pushEvent("scan_failed", {{"scan_id", scan_id},
                                      {"error_code", error_code},
                                      {"error_message", error_message},
                                      {"captured_count", std::to_string(captured)},
                                      {"current_index", std::to_string(current)}});
            return;
        }

        const auto show_timestamp = std::chrono::steady_clock::now();
        const auto expected = (index % 2 == 0) ? structured_light::sync::MarkerState::black
                                               : structured_light::sync::MarkerState::white;
        std::optional<structured_light::sync::SyncEvent> event;
        try
        {
            event = photodiode_source.waitForTransition(expected, show_timestamp,
                                                        std::chrono::milliseconds(config.sync_timeout_ms));
        }
        catch (const PhotodiodeTransportError& error)
        {
            std::lock_guard lock(mutex_);
            state_ = ScanState::failed;
            last_error_code_ = error.code();
            last_error_message_ = error.what();
            pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                       {"error_message", last_error_message_}});
            return;
        }
        if (!event)
        {
            std::lock_guard lock(mutex_);
            state_ = ScanState::failed;
            last_error_code_ = "photodiode_timeout";
            last_error_message_ = "Photodiode eventをtimeout内に受信できませんでした";
            pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                       {"error_message", last_error_message_}});
            return;
        }

        const auto selected_at = structured_light::sync::selectionTime(
            *event, std::chrono::milliseconds(config.sync_guard_ms));
        std::optional<video::FrameSample> selected_left;
        std::optional<video::FrameSample> selected_right;
        const auto selection_deadline = std::chrono::steady_clock::now() +
                                        std::chrono::milliseconds(config.sync_timeout_ms);
        while (!stop_token.stop_requested() && std::chrono::steady_clock::now() < selection_deadline &&
               (!selected_left || (right_id && !selected_right)))
        {
            if (!selected_left) selected_left = camera_service_.firstFrameAtOrAfter(*left_id, selected_at);
            if (right_id && !selected_right)
                selected_right = camera_service_.firstFrameAtOrAfter(*right_id, selected_at);
            if (!selected_left || (right_id && !selected_right))
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!selected_left || (right_id && !selected_right))
        {
            std::lock_guard lock(mutex_);
            state_ = ScanState::failed;
            last_error_code_ = "camera_frame_timeout";
            last_error_message_ = "Photodiode timestamp + guard以降のcamera frameを取得できませんでした";
            pushEvent("scan_failed", {{"scan_id", scan_id}, {"error_code", last_error_code_},
                                       {"error_message", last_error_message_}});
            return;
        }

        const auto left_path = config.output_dir / "left" / patternFileName(index);
        const auto right_path = config.output_dir / "right" / patternFileName(index);
        capture::CaptureResult left_capture_result;
        capture::CaptureResult right_capture_result;
        {
            std::lock_guard capture_lock(scan_capture_mutex_);
            left_capture_result = capture_service_.saveFrameSample(*selected_left, left_path);
            if (right_id) right_capture_result = capture_service_.saveFrameSample(*selected_right, right_path);
        }
        if (!left_capture_result.ok || (right_id && !right_capture_result.ok))
        {
            int captured = 0;
            int current = -1;
            std::string error_code;
            std::string error_message;
            {
                std::lock_guard lock(mutex_);
                state_ = ScanState::failed;
                last_error_code_ = "capture_failed";
                const auto& failed = !left_capture_result.ok ? left_capture_result : right_capture_result;
                last_error_message_ = failed.error ? failed.error->message : "frame sample save failed";
                captured = captured_count_;
                current = current_index_;
                error_code = last_error_code_;
                error_message = last_error_message_;
            }
            pushEvent("scan_failed", {{"scan_id", scan_id},
                                      {"error_code", error_code},
                                      {"error_message", error_message},
                                      {"captured_count", std::to_string(captured)},
                                      {"current_index", std::to_string(current)}});
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
                                          {"right_path", right_id ? right_path.string() : std::string{}}});
    }

    int captured = 0;
    {
        std::lock_guard lock(mutex_);
        state_ = ScanState::completed;
        current_index_ = pattern_count > 0 ? pattern_count - 1 : -1;
        captured = captured_count_;
    }
    pushEvent("scan_completed", {{"scan_id", scan_id},
                                 {"captured_count", std::to_string(captured)},
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
                                int code_width, int code_height, const projector::ProjectorSurface& surface,
                                std::string& error_message) const
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
               << "  \"sync\": \"photodiode\",\n"
               << "  \"photodiode_device\": \"" << jsonEscape(config.photodiode_device) << "\",\n"
               << "  \"photodiode_baud\": " << config.photodiode_baud << ",\n"
               << "  \"sync_timeout_ms\": " << config.sync_timeout_ms << ",\n"
               << "  \"sync_guard_ms\": " << config.sync_guard_ms << ",\n"
               << "  \"pattern_count\": " << pattern_count << ",\n"
               << "  \"projector_width\": " << code_width << ",\n"
               << "  \"projector_height\": " << code_height << ",\n"
               << "  \"output_dir\": \"" << jsonEscape(config.output_dir.string()) << "\",\n"
               << "  \"surface\": {\n"
               << "    \"monitor_index\": " << surface.monitor_index << ",\n"
               << "    \"monitor_width\": " << surface.monitor_width << ",\n"
               << "    \"monitor_height\": " << surface.monitor_height << ",\n"
               << "    \"surface_width\": " << surface.surface_width << ",\n"
               << "    \"surface_height\": " << surface.surface_height << ",\n"
               << "    \"display_width\": " << surface.pattern_width << ",\n"
               << "    \"display_height\": " << surface.pattern_height << ",\n"
               << "    \"display_x\": " << surface.pattern_x << ",\n"
               << "    \"display_y\": " << surface.pattern_y << ",\n"
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

} // namespace scan
