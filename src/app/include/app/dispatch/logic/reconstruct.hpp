#pragma once
#include "app/app_context.hpp"
#include "cmd/commands.hpp"
#include "window/window.hpp"

namespace app::dispatch::logic {

// 3D復元のメインロジックを実行する関数
void run_reconstruction(AppContext& ctx, const cmd::CmdReconstruct& c, win::Window& target_window);

} // namespace