#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct CalibrationHandlerContext;
}

namespace win
{
class Window;
}

namespace service::calibration
{

void clear(runtime::CalibrationHandlerContext& ctx, win::Window& target_window, const cmd::CmdCalibClear& command);
void capture(runtime::CalibrationHandlerContext& ctx, win::Window& target_window, const cmd::CmdCalibCapture& command);
void calibrate(runtime::CalibrationHandlerContext& ctx, const cmd::CmdCalibrate& command);

} // namespace service::calibration
