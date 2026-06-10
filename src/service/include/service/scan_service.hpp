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

namespace service::scan
{

void start(runtime::ScanHandlerContext& ctx, win::Window& target_window, const cmd::CmdStartScan& command);
void stop(runtime::ScanHandlerContext& ctx);

} // namespace service::scan
