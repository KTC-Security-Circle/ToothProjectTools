#include "runtime/handler_context.hpp"

#include "runtime/app_context.hpp"

namespace runtime
{

GlobalHandlerContext make_global_handler_context(AppContext& ctx)
{
    return {ctx.win_mgr, ctx.running, ctx.focused_id};
}

WindowHandlerContext make_window_handler_context(AppContext&)
{
    return {};
}

PatternHandlerContext make_pattern_handler_context(AppContext& ctx)
{
    return {ctx.sl_system.get()};
}


CalibrationHandlerContext make_calibration_handler_context(AppContext& ctx)
{
    return {ctx.cam_mgr, ctx.capture_service, ctx.calibrator.get(), ctx.cam_to_win, ctx.id_preview};
}

StereoCalibrationHandlerContext make_stereo_calibration_handler_context(AppContext& ctx)
{
    return {ctx.cam_mgr, ctx.stereo_calibrator.get(), ctx.stereo_data};
}

ReconstructionHandlerContext make_reconstruction_handler_context(AppContext& ctx)
{
    return {ctx.id_preview};
}

CaptureHandlerContext make_capture_handler_context(AppContext& ctx)
{
    return {ctx.capture_service};
}

} // namespace runtime
