#include "input/bindings_default.hpp"
#include "input/keycodes.hpp"
#include "logger/logger_macros.hpp"
#include "cmd/commands.hpp"
#include "cmd/target.hpp"
#include "cmd/dispatch_cmd.hpp"

// ここでは「コマンドを積むだけ」。実操作は dispatch/update 側に任せる。
void install_default_bindings(
  InputHandler& handler,
  std::deque<DispatchCmd>& cmd_que   // ★ ここを DispatchCmd に
) {
  // 終了（ESC / q / Q）: アプリ全体に対する終了要求
  auto request_quit = [&]{
    LOG_INFO("終了要求");
    cmd_que.push_back(DispatchCmd{
      Target{TargetAll{}},        // 宛先: 全ウィンドウ（アプリ全体）
      Command{CmdQuit{}}
    });
  };
  handler.bind(KEY_ESC, request_quit);
  handler.bind(KEY_Q,     request_quit);
  handler.bind(KEY_Q_UPPER,     request_quit);

  // フルスクリーン切替: フォーカス中のウィンドウ
  handler.bind(KEY_F, [&]{
    LOG_INFO("フルスクリーン切替を予約");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      Command{CmdToggleFullscreen{}}
    });
  });

  // モニタ1へ移動: フォーカス中のウィンドウ
  handler.bind('1', [&]{
    LOG_INFO("モニタ1への移動を予約");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      Command{CmdMoveToMonitor{1}}
    });
  });

  // モニタ2へ移動: フォーカス中のウィンドウ
  handler.bind('2', [&]{
    LOG_INFO("フォーカス移動を予約");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      Command{CmdMoveToMonitor{2}}
    });
  });

  handler.bind(KEY_TAB, [&]{
    LOG_INFO("モニタ2への移動を予約");
    cmd_que.push_back(DispatchCmd{
     Target{TargetFocused{}},
    Command{CmdFocusNext{}}
    });
  });

}
