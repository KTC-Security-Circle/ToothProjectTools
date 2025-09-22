#include "app/app.hpp"
#include "input/bindings_default.hpp"
#include "input/keycodes.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

App::App()
  : window_("Preview", {800, 600}, {100, 100}) {

  window_.create();
  window_.setMonitorIndex(1);

  // 画像を一度作る（例）
  image_ = cv::Mat(window_.size().height, window_.size().width,
                   CV_8UC3, cv::Scalar(30, 30, 30));
  cv::putText(image_,
              "Hello HighGUI",
              {40, 300},
              cv::FONT_HERSHEY_SIMPLEX,
              2.0,
              {200, 200, 255},
              3);

  install_default_bindings(input_, pending_);
}

void App::run() {
  while (running_) {
    processInput();
    update();
    render();
  }
}

void App::processInput() {
  // HighGUI の唯一のイベント経路。周期的に呼ぶ必要あり
  // pollKey() が使えるなら非ブロッキング。環境差が気になるなら waitKey(16) でもOK。
#if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
  const int key = cv::pollKey();
#else
  const int key = cv::waitKey(16);
#endif
  input_.handle(key);
}

void App::update() {
  bool operated = false;

  if (pending_.toggle_fullscreen) {
    pending_.toggle_fullscreen = false;
    LOG_INFO("フルスクリーン切替");
    window_.setFullscreen(!window_.fullscreen());
    operated = true;
  }
  if (pending_.move_to_monitor_1) {
    pending_.move_to_monitor_1 = false;
    window_.setMonitorIndex(1);
    operated = true;
  }
  if (pending_.move_to_monitor_2) {
    pending_.move_to_monitor_2 = false;
    window_.setMonitorIndex(2);
    operated = true;
  }
  if (pending_.quit) {
    pending_.quit = false;
    // このフレームでは描画しないで終了に向かう
    skip_render_once_ = true;
    running_ = false;               // ← ループを抜ける
    return;
  }

  if (operated) {
    // HighGUIの仕様：ウィンドウ操作直後はイベントを1tick流すと安定
    cv::waitKey(1);
    // 操作したフレームの描画はスキップ（プロパティ反映の揺れを避ける）
    skip_render_once_ = true;
  }
}

void App::render() {
  if (skip_render_once_) { skip_render_once_ = false; return; }
  window_.present(image_);
}