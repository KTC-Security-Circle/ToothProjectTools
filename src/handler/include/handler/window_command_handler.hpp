#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct WindowHandlerContext;
}

namespace win
{
class Window;
}

namespace handler::window
{

bool handle(runtime::WindowHandlerContext& ctx, win::Window& target_window, const cmd::Command& command);

} // namespace handler::window
