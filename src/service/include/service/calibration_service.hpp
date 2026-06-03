#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct AppContext;
}

namespace win
{
class Window;
}

namespace service::calibration
{

void clear(runtime::AppContext& ctx, win::Window& target_window, const cmd::CmdCalibClear& command);
void capture(runtime::AppContext& ctx, win::Window& target_window, const cmd::CmdCalibCapture& command);
void calibrate(runtime::AppContext& ctx, const cmd::CmdCalibrate& command);

} // namespace service::calibration
