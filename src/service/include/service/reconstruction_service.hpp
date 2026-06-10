#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct ReconstructionHandlerContext;
}

namespace win
{
class Window;
}

namespace service::reconstruction
{

void run(runtime::ReconstructionHandlerContext& ctx, win::Window& target_window, const cmd::CmdReconstruct& command);

} // namespace service::reconstruction
