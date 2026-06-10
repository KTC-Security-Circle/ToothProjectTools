#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct StereoCalibrationHandlerContext;
}

namespace handler::stereo_calibration
{

bool handle(runtime::StereoCalibrationHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::stereo_calibration
