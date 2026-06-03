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

namespace service::scan
{

void start(runtime::AppContext& ctx, win::Window& target_window, const cmd::CmdStartScan& command);
void stop(runtime::AppContext& ctx);

} // namespace service::scan
