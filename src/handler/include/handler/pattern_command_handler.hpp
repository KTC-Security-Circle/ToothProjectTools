#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct PatternHandlerContext;
}

namespace win
{
class Window;
}

namespace handler::pattern
{

bool handle(runtime::PatternHandlerContext& ctx, win::Window& target_window, const cmd::Command& command);

} // namespace handler::pattern
