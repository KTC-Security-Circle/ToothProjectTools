#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

#include <optional>

namespace calib { class Calibrator; class StereoCalibrator; struct StereoData; }
namespace calib::projector { class CameraProjectorCalibrationService; }
namespace capture { class CaptureService; }
namespace video { class CameraManager; }
namespace reconstruction { class ReconstructionService; }
namespace video { class CameraService; }
namespace win { class WindowService; }
namespace projector { class ProjectorService; }
namespace scan { class ScanService; }
namespace scan::dataset { class ScanDatasetValidator; }
namespace decode { class DecodeService; }
namespace stereo_scan { class StereoScanService; }

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
    HeadlessCommandExecutor(video::CameraService& camera_service,
                            win::WindowService& window_service,
                            projector::ProjectorService& projector_service,
                            scan::ScanService& scan_service,
                            scan::dataset::ScanDatasetValidator& scan_dataset_validator,
                            decode::DecodeService& decode_service,
                            capture::CaptureService& capture_service, video::CameraManager& cameras,
                            calib::Calibrator* calibrator, calib::StereoCalibrator* stereo_calibrator,
                            calib::StereoData& stereo_data,
                            reconstruction::ReconstructionService& reconstruction_service,
                            calib::projector::CameraProjectorCalibrationService& camera_projector_calibration_service,
                            stereo_scan::StereoScanService& stereo_scan_service);

    /// @return service結果を外部応答形式へ写像したcommand result。
    common::CommandResult execute(const cmd::Command& command);

  private:
    template <typename CommandType>
    std::optional<common::CommandResult> validateResourceAccess(const CommandType& command) const;

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
    common::CommandResult executeTyped(const cmd::CmdDetectCalibrationCorners& command);
    common::CommandResult executeTyped(const cmd::CmdCalibCapture& command);
    common::CommandResult executeTyped(const cmd::CmdStereoCalibrate& command);
    common::CommandResult executeTyped(const cmd::CmdValidateReconstruction& command);
    common::CommandResult executeTyped(const cmd::CmdReconstructPointCloud& command);
    common::CommandResult executeTyped(const cmd::CmdStereoScan& command);
    common::CommandResult executeTyped(const cmd::CmdCameraProjectorCalibrate& command);

    video::CameraService& camera_service_;
    win::WindowService& window_service_;
    projector::ProjectorService& projector_service_;
    scan::ScanService& scan_service_;
    scan::dataset::ScanDatasetValidator& scan_dataset_validator_;
    decode::DecodeService& decode_service_;
    capture::CaptureService& capture_service_;
    video::CameraManager& cameras_;
    calib::Calibrator* calibrator_;
    calib::StereoCalibrator* stereo_calibrator_;
    calib::StereoData& stereo_data_;
    reconstruction::ReconstructionService& reconstruction_service_;
    calib::projector::CameraProjectorCalibrationService& camera_projector_calibration_service_;
    stereo_scan::StereoScanService& stereo_scan_service_;
};

} // namespace headless
