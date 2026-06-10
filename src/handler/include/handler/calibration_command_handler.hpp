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

namespace handler::calibration
{

bool handle(runtime::CalibrationHandlerContext& ctx, win::Window& target_window, const cmd::Command& command);

} // namespace handler::calibration
