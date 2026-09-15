#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

#include <optional>

namespace calib { class Calibrator; class StereoCalibrator; struct StereoData; }
namespace capture { class CaptureService; }
namespace video { class CameraManager; }
namespace reconstruction { class ReconstructionService; }
namespace service::camera { class CameraService; }
namespace service::window { class WindowService; }
namespace service::projector { class ProjectorService; }
namespace service::scan { class ScanService; }
namespace service::scan_dataset { class ScanDatasetValidator; }
namespace service::decode { class DecodeService; }

namespace headless
{

/**
 * @brief 型付きheadless commandを対応するserviceへ直接実行する。
 *
 * command variantのroutingはexecuteで一度だけ行う。active scan中のresource競合は
 * service呼び出し前に共通policyとして検証する。
 */
class HeadlessCommandExecutor
{
  public:
    HeadlessCommandExecutor(service::camera::CameraService& camera_service,
                            service::window::WindowService& window_service,
                            service::projector::ProjectorService& projector_service,
                            service::scan::ScanService& scan_service,
                            service::scan_dataset::ScanDatasetValidator& scan_dataset_validator,
                            service::decode::DecodeService& decode_service,
                            capture::CaptureService& capture_service, video::CameraManager& cameras,
                            calib::Calibrator* calibrator, calib::StereoCalibrator* stereo_calibrator,
                            calib::StereoData& stereo_data,
                            reconstruction::ReconstructionService& reconstruction_service);

    /// @return service結果を外部応答形式へ写像したcommand result。
    common::CommandResult execute(const cmd::Command& command);

  private:
    std::optional<common::CommandResult> rejectIfScanResourceBusy(const cmd::Command& command) const;

    common::CommandResult executeTyped(const cmd::CmdOpenCamera& command);
    common::CommandResult executeTyped(const cmd::CmdCloseCamera& command);
    common::CommandResult executeTyped(const cmd::CmdOpenWindow& command);
    common::CommandResult executeTyped(const cmd::CmdCloseWindow& command);
    common::CommandResult executeTyped(const cmd::CmdListMonitors& command);
    common::CommandResult executeTyped(const cmd::CmdConfigureProjectorSurface& command);
    common::CommandResult executeTyped(const cmd::CmdOpenProjector& command);
    common::CommandResult executeTyped(const cmd::CmdCloseProjector& command);
    common::CommandResult executeTyped(const cmd::CmdGeneratePatterns& command);
    common::CommandResult executeTyped(const cmd::CmdProjectorShowPattern& command);
    common::CommandResult executeTyped(const cmd::CmdProjectorNextPattern& command);
    common::CommandResult executeTyped(const cmd::CmdProjectorPrevPattern& command);
    common::CommandResult executeTyped(const cmd::CmdCaptureFrame& command);
    common::CommandResult executeTyped(const cmd::CmdCaptureStereo& command);
    common::CommandResult executeTyped(const cmd::CmdStartScan& command);
    common::CommandResult executeTyped(const cmd::CmdScanStatus& command);
    common::CommandResult executeTyped(const cmd::CmdStopScan& command);
    common::CommandResult executeTyped(const cmd::CmdValidateScanDataset& command);
    common::CommandResult executeTyped(const cmd::CmdDecodePatterns& command);
    common::CommandResult executeTyped(const cmd::CmdCalibrate& command);
    common::CommandResult executeTyped(const cmd::CmdCalibCapture& command);
    common::CommandResult executeTyped(const cmd::CmdStereoCalibrate& command);
    common::CommandResult executeTyped(const cmd::CmdValidateReconstruction& command);
    common::CommandResult executeTyped(const cmd::CmdReconstructPointCloud& command);

    service::camera::CameraService& camera_service_;
    service::window::WindowService& window_service_;
    service::projector::ProjectorService& projector_service_;
    service::scan::ScanService& scan_service_;
    service::scan_dataset::ScanDatasetValidator& scan_dataset_validator_;
    service::decode::DecodeService& decode_service_;
    capture::CaptureService& capture_service_;
    video::CameraManager& cameras_;
    calib::Calibrator* calibrator_;
    calib::StereoCalibrator* stereo_calibrator_;
    calib::StereoData& stereo_data_;
    reconstruction::ReconstructionService& reconstruction_service_;
};

} // namespace headless
