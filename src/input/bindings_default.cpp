#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "cmd/keycodes.hpp"

/// @brief アプリ既定のキー→コマンド・バインディングを登録する。
/// @details
/// - ここでは「コマンドをキューに積むだけ」。実処理は dispatch/update 側で行う。
/// - HighGUI等の入力ループから呼ばれることを想定。
/// @param handler  バインディングを登録する InputHandler
/// @param cmd_que  コマンド配送用のキュー（先入れ先出し）
///
/// @note GUIループ側で未入力は負数（例: waitKey/pollKey が -1）を返しうる点に注意。
///       （未入力の仕様はOpenCV HighGUIのwaitKey系に準ずる）
///       詳細: 「押されたキーコード、または未入力なら -1 を返す」。:contentReference[oaicite:0]{index=0}
void install_default_bindings(
  InputHandler& handler,
  std::deque<DispatchCmd>& cmd_que
) {
  //======================================================================
  // 1) アプリ終了系（ESC / q / Q）: 宛先 = アプリ全体
  //======================================================================
  auto request_quit = [&]{
    LOG_INFO("終了要求を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetAll{}},
      Command{CmdQuit{}}
    });
  };
  handler.bind(KEY_ESC,      request_quit);
  handler.bind(KEY_Q,        request_quit);
  handler.bind(KEY_Q_UPPER,  request_quit);

  //======================================================================
  // 2) ウィンドウ制御（フルスクリーン切替）: 宛先 = フォーカス中
  //======================================================================
  handler.bind(KEY_F, [&]{
    LOG_INFO("フルスクリーン切替を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      Command{CmdToggleFullscreen{}}
    });
  });

  //======================================================================
  // 3) マルチモニタ操作（番号指定で移動）: 宛先 = フォーカス中
  //======================================================================
  handler.bind('1', [&]{
    LOG_INFO("モニタ1への移動を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      Command{CmdMoveToMonitor{1}}
    });
  });

  handler.bind('2', [&]{
    LOG_INFO("モニタ2への移動を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      Command{CmdMoveToMonitor{2}}
    });
  });

  //======================================================================
  // 4) フォーカス切替（次へ）: 宛先 = フォーカス管理
  //======================================================================
  handler.bind(KEY_TAB, [&]{
    LOG_INFO("次のウィンドウへのフォーカス移動を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      Command{CmdFocusNext{}}
    });
  });
}
