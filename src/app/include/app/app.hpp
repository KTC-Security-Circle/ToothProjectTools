#pragma once

//=== 外部依存 ===============================================================
#include <deque>
#include <vector>
#include <memory>
#include <unordered_map>

#include "window/window.hpp"
#include "window/window_manager.hpp" // Manager追加
#include "input/input_handler.hpp"
#include "cmd/dispatch_cmd.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp" // Manager追加
#include "structured_light/structured_light.hpp"

//=== 前方宣言・コンテキスト ================================================
class App;

struct MouseCallbackContext {
  App* app_ptr;
  win::WindowId window_id; // IDに変更
};

//=== アプリケーションクラス ================================================
class App {
public:
  //--- ライフサイクル -------------------------------------------------------
  App();
  void run(); // init() -> loop() を呼ぶラッパー

private:
  //--- 初期化とループ -------------------------------------------------------
  void init();           // 構築・初期化ロジック (旧コンストラクタの中身)
  void loop();           // メインループ (旧runの中身)

  //--- メインループの処理分割 ----------------------------------------------
  void processInput();   // waitKey/pollKey の隔離
  void update();         // 状態更新
  void render();         // 描画

  //--- ディスパッチ ---------------------------------------------------------
  void dispatch(const DispatchCmd& dispatch_command); 
  bool handleAppLevelCommand_(const Command& command);      
  void applyCommandToWindow_(win::Window& target_window, const Command& command);
  void dispatchToAll_(const DispatchCmd& dispatch_command);
  void dispatchToFocused_(const DispatchCmd& dispatch_command);
  void dispatchToId_(const DispatchCmd& dispatch_command, win::WindowId target_window_id);
  void finalizeDispatch_(); // 共通後処理（waitKey + 描画スキップ）

  //--- ハンドラ実体 ---------------------------------------------------------
  void doToggleFullscreen();
  void doMoveToMonitor(int index);
  void doQuit();
  void doFocusNext();

  //--- マウスコールバック ---------------------------------------------------
  static void onMouseCallback(int event, int x, int y, int flags, void* userdata);

  //--- 補助ユーティリティ ---------------------------------------------------
  // Manager導入により不要になるケースが多いが、ラッパーとして残す場合
  win::Window* findWindowById(win::WindowId id) {
    return win_mgr_.get(id);
  }

private:
  //=== 状態フィールド =======================================================
  
  //--- リソース管理 (Manager) -----------------------------------------------
  win::WindowManager   win_mgr_;
  video::CameraManager cam_mgr_;

  //--- ID保持 (アクセス用) --------------------------------------------------
  win::WindowId id_preview_{win::kInvalidWindowId};
  win::WindowId id_second_{win::kInvalidWindowId};
  win::WindowId id_projector_{win::kInvalidWindowId};
  
  win::WindowId focused_id_{win::kInvalidWindowId};

  video::CameraId id_cam1_{video::kInvalidCameraId};
  video::CameraId id_cam2_{video::kInvalidCameraId};

  // カメラID -> ウィンドウID のマッピング
  std::unordered_map<video::CameraId, win::WindowId> cam_to_win_;

  //--- コールバックコンテキスト保持 -----------------------------------------
  std::vector<MouseCallbackContext> mouse_callback_contexts_;

  //--- コマンドキュー -------------------------------------------------------
  std::deque<DispatchCmd> cmd_que_;

  //--- 入力・描画バッファ ---------------------------------------------------
  InputHandler input_;

  //--- ループ制御フラグ -----------------------------------------------------
  bool running_{true};
  bool skip_render_once_{false};

  //--- 構造光システム -------------------------------------------------------
  std::unique_ptr<sl::StructuredLight> sl_system_;

  // 構造光シーケンス管理用
  bool is_scanning_{false};       
  int current_pattern_index_{-1}; 
  std::chrono::steady_clock::time_point last_pattern_change_time_;
};
