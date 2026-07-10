#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace calib
{
class Calibrator;
class StereoCalibrator;
struct StereoData;
} // namespace calib

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
namespace camera
{
class CameraService;
}
namespace window
{
class WindowService;
}
namespace projector
{
class ProjectorService;
}
} // namespace service

namespace headless
{

class HeadlessDispatcher
{
  public:
    /// @brief GUI非依存serviceを参照してHeadlessDispatcherを構築する。
    ///
    /// Args:
    ///   camera_service <service::camera::CameraService&>: camera open/closeを実行するdomain service。
    ///   capture_service <capture::CaptureService&>: capture commandを実行するdomain service。
    ///   window_service <service::window::WindowService&>: window open/closeを実行するdomain service。
    ///   cameras <video::CameraManager&>: calibration対象cameraを取得するmanager。
    ///   calibrator <calib::Calibrator*>: mono calibration計算器。
    ///   stereo_calibrator <calib::StereoCalibrator*>: stereo calibration計算器。
    ///   stereo_data <calib::StereoData&>: stereo calibration計算結果の保存先。
    ///
    /// Return:
    ///   <HeadlessDispatcher>: GUI非依存handler contextを保持するdispatcher。
    HeadlessDispatcher(service::camera::CameraService& camera_service, service::window::WindowService& window_service,
                       service::projector::ProjectorService& projector_service, capture::CaptureService& capture_service,
                       video::CameraManager& cameras, calib::Calibrator* calibrator,
                       calib::StereoCalibrator* stereo_calibrator, calib::StereoData& stereo_data);

    /// @brief headlessで実行可能なcommandを実行する。
    ///
    /// Args:
    ///   command <const cmd::Command&>: 実行対象のcommand variant。
    ///
    /// Return:
    ///   <common::CommandResult>: commandの処理有無、成功可否、error、response用values。
    common::CommandResult execute(const cmd::Command& command);

  private:
    /// camera_service_ <service::camera::CameraService&>: camera resource commandを実行するservice。
    service::camera::CameraService& camera_service_;

    /// window_service_ <service::window::WindowService&>: window resource commandを実行するservice。
    service::window::WindowService& window_service_;

    /// projector_service_ <service::projector::ProjectorService&>: projector commandを実行するservice。
    service::projector::ProjectorService& projector_service_;

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
