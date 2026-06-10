#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct StereoCalibrationHandlerContext;
}

namespace service::stereo_calibration
{

void calibrate(runtime::StereoCalibrationHandlerContext& ctx, const cmd::CmdStereoCalibrate& command);

} // namespace service::stereo_calibration
