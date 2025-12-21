#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "cmd/commands.hpp" 
#include "cmd/keycodes.hpp"
#include "window/window.hpp"

namespace input {

void install_default_bindings(
  InputHandler& handler,
  std::deque<DispatchCmd>& cmd_que,
  win::WindowId projector_id,
  const CalibrationBindConfig& calib_config,
  TargetResolver get_focused_target
) {
  // ======================================================================
  // 1) アプリ終了系 (ESC / Ctrl+Q)
  // ======================================================================
  auto request_quit = [&]{
    LOG_INFO("終了要求を予約します");
    cmd_que.push_back(DispatchCmd{ TargetAll{}, cmd::CmdQuit{} });
  };
  handler.bind(KEY_ESC,      request_quit);
  handler.bind(CTRL_KEY('q'), request_quit); // Ctrl+Q で終了

  // ======================================================================
  // 2) ウィンドウ制御
  // ======================================================================
  // F: フルスクリーン
  handler.bind('f', [&]{
    cmd_que.push_back(DispatchCmd{ TargetFocused{}, cmd::CmdToggleFullscreen{} });
  });

  // TAB: 次のウィンドウへフォーカス
  handler.bind(KEY_TAB, [&]{
    cmd_que.push_back(DispatchCmd{ TargetFocused{}, cmd::CmdFocusNext{} });
  });

  // 1, 2: モニタ移動
  handler.bind('1', [&]{ cmd_que.push_back(DispatchCmd{ TargetFocused{}, cmd::CmdMoveToMonitor{1} }); });
  handler.bind('2', [&]{ cmd_que.push_back(DispatchCmd{ TargetFocused{}, cmd::CmdMoveToMonitor{2} }); });

  // ======================================================================
  // 3) 汎用カメラ撮影 (Ctrl+S) - ScreenShot
  // ======================================================================
  handler.bind(CTRL_KEY('s'), [&]{
    LOG_INFO("スナップショット(Focused)を予約します");
    cmd::CmdCapturePush cap{cmd::CaptureScope::FocusedOnly, std::nullopt, {}};
    cmd_que.push_back(DispatchCmd{ TargetFocused{}, cap });
  });

  // ======================================================================
  // 4) 構造光パターン制御 (P/N/B)
  // ======================================================================
  if (projector_id != win::kInvalidWindowId) {
      // p: Reset
      handler.bind('p', [&, projector_id]{
        cmd_que.push_back(DispatchCmd{ TargetById{projector_id}, cmd::CmdShowPattern{0} });
      });
      // n: Next
      handler.bind('n', [&, projector_id]{
        cmd_que.push_back(DispatchCmd{ TargetById{projector_id}, cmd::CmdNextPattern{} });
      });
      // b: Back
      handler.bind('b', [&, projector_id]{
        cmd_que.push_back(DispatchCmd{ TargetById{projector_id}, cmd::CmdPrevPattern{} });
      });
  }

  // ======================================================================
  // 5) 自動スキャン (Ctrl+Z: Start, Ctrl+X: Stop)
  // ======================================================================
  if (projector_id != win::kInvalidWindowId) {
      handler.bind(CTRL_KEY('z'), [&, projector_id]{
        LOG_INFO("自動スキャン開始");
        cmd_que.push_back(DispatchCmd{ TargetById{projector_id}, cmd::CmdStartScan{500} });
      });
  }
  
  handler.bind(CTRL_KEY('x'), [&]{
    LOG_INFO("スキャン中断");
    cmd_que.push_back(DispatchCmd{ TargetAll{}, cmd::CmdStopScan{} });
  });

  // ======================================================================
  // 6) キャリブレーション機能 (統合)
  // ======================================================================
  
  // 変数をコピーキャプチャして寿命問題を回避
  // (std::string はコピーコストがかかりますが、初期化時の1回だけなので問題ありません)
  auto cfg = calib_config; 

  // [K] Calibrate (計算実行)
  handler.bind('k', [&, cfg](){
      if (cfg.scan_cam_left != video::kInvalidCameraId) 
          cmd_que.push_back({TargetAll{}, cmd::CmdCalibrate{cfg.scan_cam_left, cfg.dir_left}});
      if (cfg.scan_cam_right != video::kInvalidCameraId) 
          cmd_que.push_back({TargetAll{}, cmd::CmdCalibrate{cfg.scan_cam_right, cfg.dir_right}});
  });

  // [Shift + C] Clear Folder (誤爆防止のためShift必須)
  // 'C' は Shift+c のコード
  handler.bind('C', [&, get_focused_target](){
      auto [cam, dir] = get_focused_target();
      if (cam != video::kInvalidCameraId) {
          cmd_que.push_back({TargetAll{}, cmd::CmdCalibClear{dir}});
      } else {
          LOG_WARN("Clear: ターゲット(カメラウィンドウ)を選択してください");
      }
  });

  // [c] Capture Single Frame (小文字)
  handler.bind('c', [&, get_focused_target](){
      auto [cam, dir] = get_focused_target();
      if (cam != video::kInvalidCameraId) {
          cmd_que.push_back({TargetAll{}, cmd::CmdCalibCapture{cam, dir}});
      } else {
          LOG_WARN("Capture: ターゲット(カメラウィンドウ)を選択してください");
      }
  });
}

} // namespace input