#pragma once
#include "app/app_context.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "cmd/commands.hpp"

namespace app::dispatch {

// コマンド処理のメインエントリ
void execute(AppContext& ctx, const DispatchCmd& dcmd);

// ★追加: キーバインドのセットアップ
void setup_bindings(AppContext& ctx);

namespace handlers {
    bool handle_global(AppContext& ctx, const cmd::Command& cmd);
    void handle_window(AppContext& ctx, win::Window& target, const cmd::Command& cmd);
}

} // namespace app::dispatch