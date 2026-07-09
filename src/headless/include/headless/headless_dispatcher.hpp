#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace calib
{
class Calibrator;
class StereoCalibrator;
struct StereoData;
}

namespace capture
{
class CaptureService;
}

namespace video
{
class CameraManager;
}

namespace service
{
class SidecarService;
}

namespace headless
{

class HeadlessDispatcher
{
  public:
    /// @brief GUI非依存serviceを参照してHeadlessDispatcherを構築する。
    ///
    /// Args:
    ///   sidecar_service <service::SidecarService&>: camera open/closeを実行する暫定service。
    ///   capture_service <capture::CaptureService&>: capture commandを実行するdomain service。
    ///   cameras <video::CameraManager&>: calibration対象cameraを取得するmanager。
    ///   calibrator <calib::Calibrator*>: mono calibration計算器。
    ///   stereo_calibrator <calib::StereoCalibrator*>: stereo calibration計算器。
    ///   stereo_data <calib::StereoData&>: stereo calibration計算結果の保存先。
    ///
    /// Return:
    ///   <HeadlessDispatcher>: GUI非依存handler contextを保持するdispatcher。
    HeadlessDispatcher(
        service::SidecarService& sidecar_service,
        capture::CaptureService& capture_service,
        video::CameraManager& cameras,
        calib::Calibrator* calibrator,
        calib::StereoCalibrator* stereo_calibrator,
        calib::StereoData& stereo_data);

    /// @brief headlessで実行可能なcommandを実行する。
    ///
    /// Args:
    ///   command <const cmd::Command&>: 実行対象のcommand variant。
    ///
    /// Return:
    ///   <common::CommandResult>: commandの処理有無、成功可否、error、response用values。
    common::CommandResult execute(const cmd::Command& command);

  private:
    /// sidecar_service_ <service::SidecarService&>: camera resource commandを実行する暫定service。
    service::SidecarService& sidecar_service_;

    /// capture_service_ <capture::CaptureService&>: capture系domain commandを実行するservice。
    capture::CaptureService& capture_service_;

    /// cameras_ <video::CameraManager&>: calibration対象cameraを取得するmanager。
    video::CameraManager& cameras_;

    /// calibrator_ <calib::Calibrator*>: mono calibration計算器。
    calib::Calibrator* calibrator_;

    /// stereo_calibrator_ <calib::StereoCalibrator*>: stereo calibration計算器。
    calib::StereoCalibrator* stereo_calibrator_;

    /// stereo_data_ <calib::StereoData&>: stereo calibration計算結果の保存先。
    calib::StereoData& stereo_data_;
};

} // namespace headless
