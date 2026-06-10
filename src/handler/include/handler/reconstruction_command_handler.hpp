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

namespace handler::reconstruction
{

bool handle(runtime::ReconstructionHandlerContext& ctx, win::Window& target_window, const cmd::Command& command);

} // namespace handler::reconstruction
