#pragma once
#include "app/app_context.hpp"

namespace app::sys {

// --- Initialization ---
void setup_app(AppContext& ctx);

// --- Core Loop Systems ---
void process_input(AppContext& ctx);
void process_commands(AppContext& ctx);
void update_scan(AppContext& ctx);
void update_preview(AppContext& ctx);
void render_all(AppContext& ctx);

// --- Input Helpers ---
// マウスコールバック実体
void on_mouse_event(int event, int x, int y, int flags, void* userdata);

} // namespace app::sys