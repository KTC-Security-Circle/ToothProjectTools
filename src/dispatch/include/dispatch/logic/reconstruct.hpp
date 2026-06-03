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

namespace dispatch::logic
{

// 3D復元のメインロジックを service へ委譲する薄い入口
void run_reconstruction(runtime::AppContext& ctx, const cmd::CmdReconstruct& c, win::Window& target_window);

} // namespace dispatch::logic
