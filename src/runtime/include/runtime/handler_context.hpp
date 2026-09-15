#pragma once

#include "video/video_types.hpp"

namespace calib
{
class Calibrator;
class StereoCalibrator;
struct StereoData;
} // namespace calib

namespace video
{
class CameraManager;
}

namespace capture
{
class CaptureService;
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
namespace scan
{
class ScanService;
}
namespace scan_dataset
{
class ScanDatasetValidator;
}
namespace decode
{
class DecodeService;
}
} // namespace service

namespace reconstruction { class ReconstructionService; }

namespace runtime
{

struct ScanHandlerContext
{
    /// scan_service <service::scan::ScanService&>: scan commandを実行するservice。
    service::scan::ScanService& scan_service;
};

struct ScanDatasetHandlerContext
{
    /// validator <service::scan_dataset::ScanDatasetValidator&>: scan dataset validator。
    service::scan_dataset::ScanDatasetValidator& validator;
};

struct DecodeHandlerContext
{
    /// decode_service <service::decode::DecodeService&>: pattern decode service。
    service::decode::DecodeService& decode_service;
};

struct MonoCalibrationCalcContext
{
    /// cameras <video::CameraManager&>: calibration対象cameraを取得するmanager。
    video::CameraManager& cameras;

    /// calibrator <calib::Calibrator*>: mono calibration計算器。
    calib::Calibrator* calibrator;
};

struct CaptureHandlerContext
{
    /// capture_service <capture::CaptureService&>: Capture commandを実行するdomain service。
    capture::CaptureService& capture_service;
};

/// @brief camera resource操作をhandlerへ渡す暫定runtime context。
struct CameraHandlerContext
{
    /// camera_service <service::camera::CameraService&>: camera open/closeを実行するdomain service。
    service::camera::CameraService& camera_service;
};

struct WindowResourceHandlerContext
{
    /// window_service <service::window::WindowService&>: window open/closeを実行するruntime service。
    service::window::WindowService& window_service;
};

struct ProjectorHandlerContext
{
    /// projector_service <service::projector::ProjectorService&>: projector commandを実行するservice。
    service::projector::ProjectorService& projector_service;
};

struct StereoCalibrationCalcContext
{
    /// cameras <video::CameraManager&>: stereo calibration対象cameraを取得するmanager。
    video::CameraManager& cameras;

    /// stereo_calibrator <calib::StereoCalibrator*>: stereo calibration計算器。
    calib::StereoCalibrator* stereo_calibrator;

    /// stereo_data <calib::StereoData&>: stereo calibration計算結果の保存先。
    calib::StereoData& stereo_data;
};

struct ReconstructionPointCloudHandlerContext { reconstruction::ReconstructionService& reconstruction_service; };

} // namespace runtime
