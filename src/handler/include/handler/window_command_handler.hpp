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

namespace handler::window
{

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command);

} // namespace handler::window
