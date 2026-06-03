#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct AppContext;
}

namespace service::stereo_calibration
{

void calibrate(runtime::AppContext& ctx, const cmd::CmdStereoCalibrate& command);

} // namespace service::stereo_calibration
