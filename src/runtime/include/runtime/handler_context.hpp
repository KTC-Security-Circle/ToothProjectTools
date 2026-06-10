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
    calib::Calibrator* calibrator;
    const std::map<video::CameraId, win::WindowId>& camera_windows;
    win::WindowId preview_window_id;
};

struct StereoCalibrationHandlerContext
{
    video::CameraManager& cameras;
    calib::StereoCalibrator* stereo_calibrator;
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

} // namespace runtime
