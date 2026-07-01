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

/// @brief Capture系commandをwindow非依存で処理する。
///
/// Args:
///   ctx <runtime::AppContext&>: CaptureServiceを含むapplication context。
///   cmd <const cmd::Command&>: dispatch対象のcommand variant。
///
/// Return:
///   <bool>: Capture系commandを処理した場合はtrue、それ以外はfalse。
bool handle_capture(runtime::AppContext& ctx, const cmd::Command& cmd);

void handle_window(runtime::AppContext& ctx, win::Window& target, const cmd::Command& cmd);
} // namespace local_handler

} // namespace dispatch
