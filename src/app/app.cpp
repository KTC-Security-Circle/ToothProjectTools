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

  install_default_bindings(input_, cmd_que_);
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
  while (!cmd_que_.empty()) {
    const Command& c = cmd_que_.front();
    std::visit([&](auto&& cmd){
      using T = std::decay_t<decltype(cmd)>;
      if constexpr (std::is_same_v<T, CmdToggleFullscreen>) {
        LOG_INFO("フルスクリーン切替");
        doToggleFullscreen(); operated = true;
      } else if constexpr (std::is_same_v<T, CmdMoveToMonitor>) {
        LOG_INFO("モニタ移動: {}", cmd.index);
        doMoveToMonitor(cmd.index); operated = true;
      } else if constexpr (std::is_same_v<T, CmdQuit>) {
        LOG_INFO("終了へ移行");
        doQuit();
      }
    }, c);
    cmd_que_.pop_front();
    if (!running_) break; // Quit発行後は抜ける
  }

  if (!running_) {        // 終了時は今フレームの描画スキップ
    skip_render_once_ = true;
    return;
  }
  if (operated) {
    cv::waitKey(1);       // HighGUI: プロパティ変更直後はイベント1tick
    skip_render_once_ = true;
  }
}

void App::render() {
  if (skip_render_once_) { skip_render_once_ = false; return; }
  window_.present(image_);
}

// 小さな処理
void App::doToggleFullscreen() { window_.setFullscreen(!window_.fullscreen()); }
void App::doMoveToMonitor(int index) { window_.setMonitorIndex(index); }
void App::doQuit() { running_ = false; }
