#pragma once

//=== 外部依存 ===============================================================
#include <deque>
#include <vector>

#include "window/window.hpp"
#include "input/input_handler.hpp"
#include "cmd/dispatch_cmd.hpp"

//=== 前方宣言・コンテキスト ================================================
class App;

struct MouseCallbackContext {
  App* app_ptr;
  win::Window::Id window_id;
};

//=== アプリケーションクラス ================================================
class App {
public:
  //--- ライフサイクル -------------------------------------------------------
  App();
  void run();

private:
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
  void dispatchToId_(const DispatchCmd& dispatch_command, WindowId target_window_id);
  void finalizeDispatch_(); // 共通後処理（waitKey + 描画スキップ）


  //--- ハンドラ実体（小さな処理に分割） ------------------------------------
  void doToggleFullscreen();
  void doMoveToMonitor(int index);
  void doQuit();
  void doFocusNext();

  //--- マウスコールバック ---------------------------------------------------
  static void onMouseCallback(int event, int x, int y, int flags, void* userdata);

  //--- 補助ユーティリティ ---------------------------------------------------
  win::Window* findWindowById(WindowId id) {
    for (auto& w : windows_) {
      if (w.id() == id) return &w;
    }
    return nullptr;
  }

private:
  //=== 状態フィールド =======================================================
  //--- ウィンドウ管理 -------------------------------------------------------
  std::vector<win::Window> windows_;
  WindowId focused_id_{0};
  std::vector<MouseCallbackContext> mouse_callback_contexts_;

  //--- コマンドキュー -------------------------------------------------------
  std::deque<DispatchCmd> cmd_que_;

  //--- 入力・描画バッファ ---------------------------------------------------
  InputHandler input_;

  //--- ループ制御フラグ -----------------------------------------------------
  bool running_{true};
  bool skip_render_once_{false};
};
