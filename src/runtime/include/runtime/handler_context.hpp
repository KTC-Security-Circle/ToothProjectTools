#pragma once

#include "video/video_types.hpp"
#include "window/window_types.hpp"

#include <map>

namespace calib
{
class Calibrator;
class StereoCalibrator;
struct StereoData;
} // namespace calib

namespace sl
{
class StructuredLight;
}

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

namespace win
{
class WindowManager;
}

namespace reconstruction { class ReconstructionService; }

namespace runtime
{

struct AppContext;

struct GlobalHandlerContext
{
    win::WindowManager& windows;
    bool& running;
    win::WindowId& focused_id;
};

struct WindowHandlerContext
{
};

struct PatternHandlerContext
{
    sl::StructuredLight* structured_light;
};

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

struct CalibrationHandlerContext
{
    video::CameraManager& cameras;
    /// capture_service <capture::CaptureService&>: calibration画像保存を実行するCapture用domain service。
    capture::CaptureService& capture_service;
    calib::Calibrator* calibrator;
    const std::map<video::CameraId, win::WindowId>& camera_windows;
    win::WindowId preview_window_id;
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

struct StereoCalibrationHandlerContext
{
    video::CameraManager& cameras;
    calib::StereoCalibrator* stereo_calibrator;
    calib::StereoData& stereo_data;
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

struct ReconstructionHandlerContext
{
    win::WindowId preview_window_id;
};

GlobalHandlerContext make_global_handler_context(AppContext& ctx);
WindowHandlerContext make_window_handler_context(AppContext& ctx);
PatternHandlerContext make_pattern_handler_context(AppContext& ctx);
CalibrationHandlerContext make_calibration_handler_context(AppContext& ctx);
StereoCalibrationHandlerContext make_stereo_calibration_handler_context(AppContext& ctx);
ReconstructionHandlerContext make_reconstruction_handler_context(AppContext& ctx);

/// @brief AppContextからCaptureHandlerContextを作成する。
///
/// Args:
///   ctx <AppContext&>: CaptureServiceを所有するapplication context。
///
/// Return:
///   <CaptureHandlerContext>: CaptureHandlerへ渡すcontext。
CaptureHandlerContext make_capture_handler_context(AppContext& ctx);

} // namespace runtime
