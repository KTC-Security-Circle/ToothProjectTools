#include "input/bindings_default.hpp"
#include "input/keycodes.hpp"
#include "logger/logger_macros.hpp"

// ここでは「フラグを立てるだけ」。実操作は update() に任せる。
void install_default_bindings(InputHandler& handler, PendingOps& pending_ops) {
  // 終了（ESC / q / Q）
  auto request_quit = [&pending_ops]{
    LOG_INFO("終了要求");
    pending_ops.quit = true;
  };
  handler.bind(KEY_ESC,     request_quit);
  handler.bind('q',         request_quit);
  handler.bind('Q',         request_quit);

  // フルスクリーン切替
  handler.bind('f', [&pending_ops]{
    LOG_INFO("フルスクリーン切替を予約");
    pending_ops.toggle_fullscreen = true;
  });

  // モニタ1へ移動
  handler.bind('1', [&pending_ops]{
    LOG_INFO("モニタ1への移動を予約");
    pending_ops.move_to_monitor_1 = true;
  });

  // モニタ2へ移動
  handler.bind('2', [&pending_ops]{
    LOG_INFO("モニタ2への移動を予約");
    pending_ops.move_to_monitor_2 = true;
  });
}
