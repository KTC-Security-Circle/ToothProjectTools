#include "input/bindings_default.hpp"
#include "input/keycodes.hpp"
#include "logger/logger_macros.hpp"

// ここでは「フラグを立てるだけ」。実操作は update() に任せる。
void install_default_bindings(
  InputHandler& handler,
  std::deque<Command>& cmd_que
) {
  // 終了（ESC / q / Q）
  auto request_quit = [&]{
    LOG_INFO("終了要求");
    cmd_que.push_back(CmdQuit{});
  };
  handler.bind(KEY_ESC,     request_quit);
  handler.bind('q',         request_quit);
  handler.bind('Q',         request_quit);

  // フルスクリーン切替
  handler.bind('f', [&]{
    LOG_INFO("フルスクリーン切替を予約");
    cmd_que.push_back(CmdToggleFullscreen{});
  });

  // モニタ1へ移動
  handler.bind('1', [&]{
    LOG_INFO("モニタ1への移動を予約");
    cmd_que.push_back(CmdMoveToMonitor{1});
  });

  // モニタ2へ移動
  handler.bind('2', [&]{
    LOG_INFO("モニタ2への移動を予約");
    cmd_que.push_back(CmdMoveToMonitor{2});
  });
}
