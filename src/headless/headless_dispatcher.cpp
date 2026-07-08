#include "headless/headless_dispatcher.hpp"

#include "capture/capture_service.hpp"
#include "handler/calibration_command_handler.hpp"
#include "handler/capture_command_handler.hpp"
#include "handler/stereo_calibration_command_handler.hpp"
#include "runtime/handler_context.hpp"

namespace headless
{

HeadlessDispatcher::HeadlessDispatcher(
    capture::CaptureService& capture_service,
    video::CameraManager& cameras,
    calib::Calibrator* calibrator,
    calib::StereoCalibrator* stereo_calibrator,
    calib::StereoData& stereo_data)
    : capture_service_(capture_service),
      cameras_(cameras),
      calibrator_(calibrator),
      stereo_calibrator_(stereo_calibrator),
      stereo_data_(stereo_data)
{
}

common::CommandResult HeadlessDispatcher::execute(const cmd::Command& command)
{
    runtime::CaptureHandlerContext capture_ctx{capture_service_};
    const auto capture_result = handler::capture::handle(capture_ctx, command);
    if (capture_result.handled)
    {
        return capture_result;
    }

    runtime::MonoCalibrationCalcContext mono_ctx{cameras_, calibrator_};
    const auto mono_result = handler::calibration::handle(mono_ctx, command);
    if (mono_result.handled)
    {
        return mono_result;
    }

    runtime::StereoCalibrationCalcContext stereo_ctx{cameras_, stereo_calibrator_, stereo_data_};
    const auto stereo_result = handler::stereo_calibration::handle(stereo_ctx, command);
    if (stereo_result.handled)
    {
        return stereo_result;
    }

    return common::notHandled();
}

} // namespace headless
