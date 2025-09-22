#pragma once
#include <deque>
#include <opencv2/core/mat.hpp> 
#include "window/window.hpp"
#include "input/input_handler.hpp"
#include "input/commands.hpp"

class App {
public:
  App();
  void run();

private:
  void processInput(); // waitKey/pollKey をここに隔離（HighGUIの仕様）
  void update();       // 状態更新（必要になったら実装）
  void render();       // 描画

private:
  win::Window window_;
  std::deque<Command> cmd_que_;
  bool running_{true};
  bool skip_render_once_{false};
  InputHandler input_;
  cv::Mat image_;

  // 小さな処理関数に分割（見通し◎）
  void doToggleFullscreen();
  void doMoveToMonitor(int index);
  void doQuit();
};
