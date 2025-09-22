#pragma once
#include <deque>
#include <opencv2/core/mat.hpp> 
#include "window/window.hpp"
#include "input/input_handler.hpp"
#include "cmd/dispatch_cmd.hpp"

class App;

struct MouseCallbackContext {
  App* app_ptr;
  win::Window::Id window_id;
};

class App {
public:
  App();
  void run();

private:
  void processInput(); // waitKey/pollKey をここに隔離（HighGUIの仕様）
  void update();       // 状態更新（必要になったら実装）
  void render();       // 描画

private:
  std::vector<win::Window> windows_;
  WindowId focused_id_{0};
  std::deque<DispatchCmd> cmd_que_;
  std::vector<MouseCallbackContext> mouse_callback_contexts_;

  static void onMouseCallback(int event, int x, int y, int flags, void* userdata);

  win::Window* findWindowById(WindowId id) {
    for (auto& w_ptr : windows_)       // windows_ 内の全要素を順番に見る
      if (w_ptr.id() == id)          // id が一致したら
        return &w_ptr;             // その Window オブジェクトへのポインタを返す
    return nullptr;                // 見つからなければヌルポインタ
  }

  bool running_{true};
  bool skip_render_once_{false};
  InputHandler input_;
  cv::Mat image_;

  void dispatch(const DispatchCmd& d); // 宛先解決＋適用

  // 小さな処理関数に分割（見通し◎）
  void doToggleFullscreen();
  void doMoveToMonitor(int index);
  void doQuit();
  void doFocusNext();
};
