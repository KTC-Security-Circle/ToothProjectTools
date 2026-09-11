#pragma once

#include "service/scan_event.hpp"
#include "service/scan_result.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <cstdint>

namespace capture
{
class CaptureService;
}

namespace service::camera
{
class CameraService;
}

namespace service::projector
{
class ProjectorService;
struct ProjectorSurface;
} // namespace service::projector

namespace service::scan
{

struct ScanStartConfig
{
    /// scan_id <std::optional<std::string>>: scan session識別子。未指定時は自動生成する。
    std::optional<std::string> scan_id;

    /// projector_role <std::string>: pattern照射に使用するprojector role名。
    std::string projector_role;

    /// left_role <std::string>: 左camera role名。
    std::string left_role;

    /// right_role <std::string>: 右camera role名。
    std::string right_role;

    /// output_dir <std::filesystem::path>: scan dataset保存先directory。
    std::filesystem::path output_dir;

    /// settle_ms <int>: pattern表示後、captureまで待機する時間ms。
    int settle_ms{120};

    /// sync_source <string>: fixed_delay / camera_roi / photodiode。未指定相当はfixed_delay。
    std::string sync_source{"fixed_delay"};
    int sync_timeout_ms{1000};
    int sync_guard_ms{30};
    int sync_stable_frames{3};
    int roi_x{0};
    int roi_y{0};
    int roi_width{32};
    int roi_height{32};
    int roi_black_threshold{40};
    int roi_white_threshold{180};
};

class ScanService
{
  public:
    ScanService(service::projector::ProjectorService& projector_service, capture::CaptureService& capture_service,
                service::camera::CameraService& camera_service, ScanEventSink& event_sink);
    ~ScanService();

    ScanService(const ScanService&) = delete;
    ScanService& operator=(const ScanService&) = delete;

    /// @brief scanを非同期開始する。
    ScanResult startScan(const ScanStartConfig& config);

    /// @brief scan状態を取得する。
    ScanResult scanStatus(std::optional<std::string> scan_id = std::nullopt) const;

    /// @brief 実行中scanへ停止要求を出す。
    ScanResult stopScan(std::optional<std::string> scan_id = std::nullopt);

    /// @brief shutdown時にscan workerを停止する。
    void shutdown();

    /// @brief scanがrunning/stopping状態か返す。
    bool isScanActive() const;

    /// @brief scan中のprojector roleか返す。
    bool isProjectorRoleBusy(const std::string& projector_role) const;

    /// @brief scan中のcamera roleか返す。
    bool isCameraRoleBusy(const std::string& camera_role) const;

    /// @brief scan中のwindow roleか返す。
    bool isWindowRoleBusy(const std::string& window_role) const;

  private:
    void workerLoop(std::stop_token stop_token, ScanStartConfig config, std::string scan_id, int pattern_count);
    ScanResult snapshotLocked() const;
    void pushEvent(std::string event, std::map<std::string, std::string> values);
    bool writeMetadata(const ScanStartConfig& config, const std::string& scan_id, int pattern_count, int code_width,
                       int code_height, const service::projector::ProjectorSurface& surface,
                       std::string& error_message) const;
    static std::string generateScanId();
    static std::string patternFileName(int index);

    service::projector::ProjectorService& projector_service_;
    capture::CaptureService& capture_service_;
    service::camera::CameraService& camera_service_;
    ScanEventSink& event_sink_;

    mutable std::mutex mutex_;
    std::mutex scan_capture_mutex_;
    std::jthread worker_;

    ScanState state_{ScanState::idle};
    std::string active_scan_id_;
    ScanStartConfig active_config_;
    /// active_window_role_ <std::string>: scan中projectorの表示先window role。
    std::string active_window_role_;
    int pattern_count_{0};
    int captured_count_{0};
    int current_index_{-1};
    std::string last_error_code_;
    std::string last_error_message_;
};

} // namespace service::scan
