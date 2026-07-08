#pragma once

#include "video/video_types.hpp"
#include "window/window_types.hpp"

#include <map>

namespace calib
{
class Calibrator;
class StereoCalibrator;
struct StereoData;
}

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

namespace win
{
class WindowManager;
}

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
    win::WindowManager& windows;
    video::CameraManager& cameras;
    /// capture_service <capture::CaptureService&>: scan中の画像保存を実行するCapture用domain service。
    capture::CaptureService& capture_service;
    sl::StructuredLight* structured_light;
    video::CameraId left_camera_id;
    video::CameraId right_camera_id;
    win::WindowId preview_window_id;
    win::WindowId second_window_id;
    int& interval_ms;
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

struct ReconstructionHandlerContext
{
    win::WindowId preview_window_id;
};

GlobalHandlerContext make_global_handler_context(AppContext& ctx);
WindowHandlerContext make_window_handler_context(AppContext& ctx);
PatternHandlerContext make_pattern_handler_context(AppContext& ctx);
ScanHandlerContext make_scan_handler_context(AppContext& ctx);
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
