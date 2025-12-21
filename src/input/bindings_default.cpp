#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "cmd/commands.hpp" 
#include "cmd/keycodes.hpp"

namespace input {

/// @brief アプリ既定のキー→コマンド・バインディングを登録する。
/// @details
/// - ここでは「コマンドをキューに積むだけ」。実処理は dispatch/update 側で行う。
/// - HighGUI等の入力ループから呼ばれることを想定。
/// @param handler  バインディングを登録する InputHandler
/// @param cmd_que  コマンド配送用のキュー（先入れ先出し）
///
/// @note GUIループ側で未入力は負数（例: waitKey/pollKey が -1）を返しうる点に注意。
///       （未入力の仕様は OpenCV HighGUI の waitKey 系に準ずる）
void install_default_bindings(
  InputHandler& handler,
  std::deque<DispatchCmd>& cmd_que,
  win::WindowId projector_id
) {
  //======================================================================
  // 1) アプリ終了系（ESC / q / Q）: 宛先 = アプリ全体
  //======================================================================
  auto request_quit = [&]{
    LOG_INFO("終了要求を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetAll{}},
      cmd::Command{cmd::CmdQuit{}}
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
      cmd::Command{cmd::CmdToggleFullscreen{}}
    });
  });

  //======================================================================
  // 3) マルチモニタ操作（番号指定で移動）: 宛先 = フォーカス中
  //======================================================================
  handler.bind('1', [&]{
    LOG_INFO("モニタ1への移動を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      cmd::Command{cmd::CmdMoveToMonitor{1}}
    });
  });

  handler.bind('2', [&]{
    LOG_INFO("モニタ2への移動を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      cmd::Command{cmd::CmdMoveToMonitor{2}}
    });
  });

  //======================================================================
  // 4) フォーカス切替（次へ）: 宛先 = フォーカス管理
  //======================================================================
  handler.bind(KEY_TAB, [&]{
    LOG_INFO("次のウィンドウへのフォーカス移動を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      cmd::Command{cmd::CmdFocusNext{}}
    });
  });

  //======================================================================
  // 5) カメラ: プッシュ撮影（フォーカスのみ）: 宛先 = フォーカス中
  //======================================================================
  handler.bind('c', [&]{
    LOG_INFO("プッシュ撮影（フォーカスのみ）を予約します");
    cmd::CmdCapturePush cap{cmd::CaptureScope::FocusedOnly, std::nullopt, {}};
    cmd_que.push_back(DispatchCmd{
      Target{TargetFocused{}},
      cmd::Command{cap}
    });
  });

  //======================================================================
  // 6) カメラ: プッシュ撮影（camera_id グループ）:
  //    宛先 = フォーカス中ウィンドウの camera_id に紐づく全ウィンドウ
  //======================================================================
  handler.bind('a', [&]{
    LOG_INFO("プッシュ撮影（camera_idグループ）を予約します");
    cmd::CmdCapturePush cap{cmd::CaptureScope::CameraGroup, std::nullopt, {}};
    cmd_que.push_back(DispatchCmd{
      Target{TargetAll{}},
      cmd::Command{cap}
    });
  });
  //======================================================================
  // 7) 構造光パターン制御 (P/N/B)
  //    宛先 = 指定されたプロジェクタID
  //======================================================================
  
  // 'P': 最初のパターンへリセット (ShowPattern 0)
  handler.bind('p', [&, projector_id]{
    if (projector_id == win::kInvalidWindowId) return;
    LOG_INFO("パターンリセット(0)を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetById{projector_id}},
      cmd::Command{cmd::CmdShowPattern{0}}
    });
  });

  // 'n': 次のパターン (Next)
  handler.bind('n', [&, projector_id]{
    if (projector_id == win::kInvalidWindowId) return;
    LOG_INFO("次のパターンを予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetById{projector_id}},
      cmd::Command{cmd::CmdNextPattern{}}
    });
  });

  // 'b': 前のパターン (Back)
  handler.bind('b', [&, projector_id]{
    if (projector_id == win::kInvalidWindowId) return;
    LOG_INFO("前のパターンを予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetById{projector_id}},
      cmd::Command{cmd::CmdPrevPattern{}}
    });
  });

  //======================================================================
  // 8) 自動スキャン制御 (Z/X)
  //    宛先 = プロジェクタID (または全体)
  //======================================================================

  // 'z': スキャン開始 (Start Scan) - 以前の'c'/'z'と競合しないよう注意
  // ※ 既存コードで 'z' が CaptureGroup に割り当てられている場合、キーを変更するか上書きします。
  //   ここでは 'S' (Start) に変更する例を示します。
  handler.bind('z', [&, projector_id]{
    if (projector_id == win::kInvalidWindowId) return;
    LOG_INFO("自動スキャン開始を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetById{projector_id}},
      cmd::Command{cmd::CmdStartScan{500}} // 500ms
    });
  });

  // 'x': スキャン中断 (Stop)
  handler.bind('x', [&]{
    LOG_INFO("スキャン中断を予約します");
    cmd_que.push_back(DispatchCmd{
      Target{TargetAll{}}, 
      cmd::Command{cmd::CmdStopScan{}}
    });
  });
}

}
