#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct ScanHandlerContext;
}

namespace win
{
class Window;
}

namespace handler::scan
{

bool handle(runtime::ScanHandlerContext& ctx, win::Window& target_window, const cmd::Command& command);

} // namespace handler::scan
