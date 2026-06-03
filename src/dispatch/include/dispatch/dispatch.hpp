#pragma once
#include "cmd/commands.hpp"
#include "cmd/dispatch_cmd.hpp"

namespace runtime
{
struct AppContext;
}

namespace win
{
class Window;
}

namespace dispatch
{

// コマンド処理のメインエントリ
void execute(runtime::AppContext& ctx, const DispatchCmd& dcmd);

void setup_bindings(runtime::AppContext& ctx);

namespace local_handler
{
bool handle_global(runtime::AppContext& ctx, const cmd::Command& cmd);
void handle_window(runtime::AppContext& ctx, win::Window& target, const cmd::Command& cmd);
} // namespace local_handler

} // namespace dispatch
