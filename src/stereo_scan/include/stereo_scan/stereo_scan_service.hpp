#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace video { class CameraService; }
namespace win { class WindowService; }
namespace projector { class ProjectorService; }
namespace scan { class ScanService; class ScanEventSink; }
namespace decode { class DecodeService; }
namespace reconstruction { class ReconstructionService; }

namespace stereo_scan
{
using StreamOperation = std::function<common::CommandResult(const std::string&)>;

class StereoScanService
{
  public:
    StereoScanService(video::CameraService&, win::WindowService&, projector::ProjectorService&,
                      scan::ScanService&, decode::DecodeService&, reconstruction::ReconstructionService&,
                      scan::ScanEventSink&, StreamOperation start_stream, StreamOperation stop_stream);
    ~StereoScanService();
    common::CommandResult start(const cmd::CmdStereoScan& command);
    void shutdown();

  private:
    void workerLoop(std::stop_token, cmd::CmdStereoScan, std::string, std::string);
    void fail(const cmd::CmdStereoScan&, const std::string& stage,
              const std::string& code, const std::string& message, bool cleanup);
    bool validateCalibration(const std::filesystem::path&, std::string&, std::string&) const;
    void push(std::string event, std::map<std::string, std::string> values);

    video::CameraService& cameras_;
    win::WindowService& windows_;
    projector::ProjectorService& projectors_;
    scan::ScanService& scans_;
    decode::DecodeService& decoder_;
    reconstruction::ReconstructionService& reconstruction_;
    scan::ScanEventSink& events_;
    StreamOperation start_stream_;
    StreamOperation stop_stream_;
    std::mutex mutex_;
    std::jthread worker_;
    bool active_{false};
};
}
