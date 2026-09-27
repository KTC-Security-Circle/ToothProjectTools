#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace video { class CameraService; }
namespace win { class WindowService; class MonitorService; }
namespace projector { class ProjectorService; }
namespace scan { class ScanService; class ScanEventSink; }
namespace decode { class DecodeService; }
namespace reconstruction { class ReconstructionService; }

namespace stereo_scan
{
struct OperationResult
{
    bool ok{false};
    std::string error_code;
    std::string error_message;
    static OperationResult success() { return {true, {}, {}}; }
    static OperationResult failure(std::string code, std::string message)
    { return {false, std::move(code), std::move(message)}; }
};

struct EnsureResult : OperationResult
{
    bool created{false};
    std::string value;
};

struct MonitorSizeResult : OperationResult
{
    int width{0};
    int height{0};
};

struct ReconstructionOutput : OperationResult
{
    std::filesystem::path ply_file;
    std::size_t point_count{0};
};

struct StereoScanOwnedResources
{
    bool opened_left_camera{false};
    bool opened_right_camera{false};
    bool started_left_stream{false};
    bool started_right_stream{false};
    bool opened_window{false};
    bool opened_projector{false};
};

/// StereoScanServiceのhardware/domain boundary。productionは既存serviceへ委譲し、testはfake化する。
class StereoScanBackend
{
  public:
    virtual ~StereoScanBackend() = default;
    virtual EnsureResult ensureCamera(const std::string& role, int device_index) = 0;
    virtual EnsureResult ensureStream(const std::string& role) = 0;
    virtual MonitorSizeResult monitorSize(int monitor_index) = 0;
    virtual OperationResult openWindow(const cmd::CmdStereoScan&, int display_width, int display_height) = 0;
    virtual OperationResult openProjector(const cmd::CmdStereoScan&) = 0;
    virtual OperationResult configureSurface(const cmd::CmdStereoScan&, int display_width, int display_height) = 0;
    virtual OperationResult generatePatterns(const cmd::CmdStereoScan&) = 0;
    virtual OperationResult runScan(const cmd::CmdStereoScan&, const std::filesystem::path& scan_dir,
                                    std::stop_token) = 0;
    virtual OperationResult decode(const cmd::CmdStereoScan&, const std::filesystem::path& scan_dir,
                                   const std::filesystem::path& decode_dir) = 0;
    virtual ReconstructionOutput reconstruct(const cmd::CmdStereoScan&,
                                              const std::filesystem::path& decode_dir) = 0;
    virtual void closeProjector(const std::string& role) = 0;
    virtual void closeWindow(const std::string& role) = 0;
    virtual void stopStream(const std::string& role) = 0;
    virtual void closeCamera(const std::string& role) = 0;
    virtual void requestStopScan(const std::string& scan_id) = 0;
};

using StreamOperation = std::function<common::CommandResult(const std::string&)>;
using StreamLookup = std::function<std::optional<std::string>(const std::string&)>;

class StereoScanService
{
  public:
    StereoScanService(video::CameraService&, win::WindowService&, win::MonitorService&,
                      projector::ProjectorService&, scan::ScanService&, decode::DecodeService&,
                      reconstruction::ReconstructionService&, scan::ScanEventSink&,
                      StreamOperation start_stream, StreamOperation stop_stream, StreamLookup stream_lookup);
    StereoScanService(StereoScanBackend&, scan::ScanEventSink&);
    ~StereoScanService();
    common::CommandResult start(const cmd::CmdStereoScan& command);
    void requestShutdown();
    void shutdown();
    bool isActive() const;

  private:
    void workerLoop(std::stop_token, cmd::CmdStereoScan, std::string, std::string,
                    StereoScanOwnedResources, int display_width, int display_height);
    void cleanup(const cmd::CmdStereoScan&, const StereoScanOwnedResources&, bool cleanup_camera_stream);
    void fail(const cmd::CmdStereoScan&, const StereoScanOwnedResources&, const std::string& stage,
              const std::string& code, const std::string& message, bool cleanup_camera_stream);
    bool validateCalibration(const std::filesystem::path&, std::string&, std::string&) const;
    void push(std::string event, std::map<std::string, std::string> values);
    void setInactive();

    std::unique_ptr<StereoScanBackend> owned_backend_;
    StereoScanBackend& backend_;
    scan::ScanEventSink& events_;
    mutable std::mutex mutex_;
    std::jthread worker_;
    bool active_{false};
    std::string active_scan_id_;
};
}
