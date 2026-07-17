#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "cmd/commands.hpp" 
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
  // 1) アプリ終了系 (ESC / q)
  // ======================================================================
  auto request_quit = [&]{
    LOG_INFO("終了要求を予約します");
    cmd_que.push_back(DispatchCmd{cmd::TargetAll{}, cmd::CmdQuit{}});
  };
  handler.bind(27, request_quit);  // ESC
  handler.bind('q', request_quit); // q

  // ======================================================================
  // 2) ウィンドウ制御
  // ======================================================================
  // [f] フルスクリーン
  handler.bind('f', [&]{
    cmd_que.push_back(DispatchCmd{cmd::TargetFocus{}, cmd::CmdToggleFullscreen{}});
  });

  // [TAB] (9) 次のウィンドウへフォーカス
  handler.bind(9, [&]{
    cmd_que.push_back(DispatchCmd{cmd::TargetFocus{}, cmd::CmdFocusNext{}});
  });

  // [1], [2] モニタ移動
  handler.bind('1', [&]{ cmd_que.push_back(DispatchCmd{cmd::TargetFocus{}, cmd::CmdMoveToMonitor{1}}); });
  handler.bind('2', [&]{ cmd_que.push_back(DispatchCmd{cmd::TargetFocus{}, cmd::CmdMoveToMonitor{2}}); });

  // ======================================================================
  // 3) キャリブレーション機能 (Mono / Stereo 分離)
  // ======================================================================
  auto cfg = calib_config; 

  // --- A. Mono (単眼) ---
  // 対象: フォーカスしているウィンドウのカメラ
  // 保存先: mono_L / mono_R

  // [c] Capture Mono
  handler.bind('c', [&, get_focused_target](){
      auto [cam, dir] = get_focused_target();
      if (cam != video::kInvalidCameraId) {
          cmd_que.push_back({cmd::TargetCamera{cam}, cmd::CmdCalibCapture{cam, dir}});
      } else {
          LOG_WARN("MonoCapture: ターゲットを選択してください");
      }
  });

  // [d] Delete Mono
  handler.bind('d', [&, get_focused_target](){
      auto [cam, dir] = get_focused_target();
      if (cam != video::kInvalidCameraId) {
          LOG_INFO("MonoClear: {}", dir);
          cmd_que.push_back({cmd::TargetAll{}, cmd::CmdCalibClear{dir}});
      } else {
          LOG_WARN("MonoClear: ターゲットを選択してください");
      }
  });

  // [k] Calc Mono
  handler.bind('k', [&, cfg](){
      if (cfg.scan_cam_left != video::kInvalidCameraId) 
          cmd_que.push_back(
              {cmd::TargetCamera{cfg.scan_cam_left},
               cmd::CmdCalibrate{cfg.scan_cam_left, cfg.dir_mono_left, cfg.dir_mono_left + ".yml", ""}});
      if (cfg.scan_cam_right != video::kInvalidCameraId) 
          cmd_que.push_back(
              {cmd::TargetCamera{cfg.scan_cam_right},
               cmd::CmdCalibrate{cfg.scan_cam_right, cfg.dir_mono_right, cfg.dir_mono_right + ".yml", ""}});
  });

  // --- B. Stereo (ステレオ) ---
  // 対象: 左右カメラ同時
  // 保存先: stereo_L / stereo_R

  // [e] Capture Stereo (Extrinsics) -> 同時撮影
  handler.bind('e', [&, cfg](){
      if (cfg.scan_cam_left != video::kInvalidCameraId && cfg.scan_cam_right != video::kInvalidCameraId) {
          LOG_INFO("StereoCapture: 同時撮影");
          cmd_que.push_back(
              {cmd::TargetCamera{cfg.scan_cam_left},
               cmd::CmdCalibCapture{cfg.scan_cam_left, cfg.dir_stereo_left}});
          cmd_que.push_back(
              {cmd::TargetCamera{cfg.scan_cam_right},
               cmd::CmdCalibCapture{cfg.scan_cam_right, cfg.dir_stereo_right}});
      }
  });

  // [r] Reset Stereo Folder -> 同時クリア
  handler.bind('r', [&, cfg](){
      LOG_INFO("StereoClear: フォルダリセット");
      cmd_que.push_back({cmd::TargetAll{}, cmd::CmdCalibClear{cfg.dir_stereo_left}});
      cmd_que.push_back({cmd::TargetAll{}, cmd::CmdCalibClear{cfg.dir_stereo_right}});
  });

  // [s] Calc Stereo
  handler.bind('s', [&, cfg](){
      if (cfg.scan_cam_left != video::kInvalidCameraId && cfg.scan_cam_right != video::kInvalidCameraId) {
          LOG_INFO("StereoCalc: 計算要求");
          cmd_que.push_back(DispatchCmd{
              cmd::TargetAll{},
              cmd::CmdStereoCalibrate{
                  cfg.scan_cam_left, cfg.scan_cam_right,
                  cfg.dir_stereo_left, cfg.dir_stereo_right, // stereoフォルダを使う
                  "calibration_stereo.yml",
                  "",
                  "",
                  cfg.dir_mono_left + ".yml",
                  cfg.dir_mono_right + ".yml",
                  false
              }
          });
      }
  });

  // ======================================================================
  // 4) 構造光パターン制御 (p/n/b)
  // ======================================================================
  if (projector_id != win::kInvalidWindowId) {
      // [p] Reset
      handler.bind('p', [&, projector_id]{
        cmd_que.push_back(DispatchCmd{cmd::TargetWindow{projector_id}, cmd::CmdShowPattern{0}});
      });
      // [n] Next
      handler.bind('n', [&, projector_id]{
        cmd_que.push_back(DispatchCmd{cmd::TargetWindow{projector_id}, cmd::CmdNextPattern{}});
      });
      // [b] Back
      handler.bind('b', [&, projector_id]{
        cmd_que.push_back(DispatchCmd{cmd::TargetWindow{projector_id}, cmd::CmdPrevPattern{}});
      });

      // ==================================================================
      // 5) 自動スキャン (Space / x)
      // ==================================================================
      
      // [Space] (32) 自動スキャン開始
      // Ctrl+Z が効かない環境のため Space に割り当て
      handler.bind(32, [&, projector_id]{
        LOG_INFO("自動スキャン開始");
        cmd_que.push_back(DispatchCmd{cmd::TargetWindow{projector_id}, cmd::CmdStartScan{std::nullopt, "projector", "left", "right", "./data/scan/default", 500}});
      });
  }
  
  // [x] スキャン中断
  // Ctrl+X が効かない環境のため x に割り当て
  handler.bind('x', [&, projector_id]{
    LOG_INFO("スキャン中断");
    cmd_que.push_back(DispatchCmd{cmd::TargetWindow{projector_id}, cmd::CmdStopScan{}});
  });

  // [m] 3D Reconstruction
  handler.bind('m', [&](){
      LOG_INFO("3D復元を開始します");
      cmd_que.push_back(DispatchCmd{
          cmd::TargetAll{},
          cmd::CmdReconstruct{
              "calibration_stereo.yml", // キャリブレーションファイル
              "captures/scan_L",        // 左画像フォルダ
              "captures/scan_R",        // 右画像フォルダ
              "reconstruction.ply"      // 出力ファイル
          }
      });
  });
}

} // namespace input
