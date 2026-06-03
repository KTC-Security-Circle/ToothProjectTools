#pragma once
#include "runtime/app_context.hpp"

namespace app::sys
{

// --- Initialization ---
void setup_app(runtime::AppContext& ctx);

// --- Core Loop Systems ---
void process_input(runtime::AppContext& ctx);
void process_commands(runtime::AppContext& ctx);
void update_scan(runtime::AppContext& ctx);
void update_preview(runtime::AppContext& ctx);
void render_all(runtime::AppContext& ctx);

// --- Input Helpers ---
// マウスコールバック実体
void on_mouse_event(int event, int x, int y, int flags, void* userdata);

} // namespace app::sys