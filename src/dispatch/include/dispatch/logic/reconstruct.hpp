#pragma once
#include "cmd/commands.hpp"
#include "runtime/app_context.hpp"
#include "window/window.hpp"

namespace dispatch::logic
{

// 3D復元のメインロジックを実行する関数
void run_reconstruction(runtime::AppContext& ctx, const cmd::CmdReconstruct& c, win::Window& target_window);

} // namespace app::dispatch::logic