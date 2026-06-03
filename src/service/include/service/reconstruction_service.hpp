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

namespace service::reconstruction
{

void run(runtime::AppContext& ctx, win::Window& target_window, const cmd::CmdReconstruct& command);

} // namespace service::reconstruction
