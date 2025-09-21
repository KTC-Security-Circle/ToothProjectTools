// src/main.cpp
#include "window/window.hpp"
#include <opencv2/imgproc.hpp>

int main() {
  // ウィンドウ生成
  win::Window window{"Preview", {800, 600}, {100, 100}};
  window.create();
  window.setMonitorIndex(2);

  // フルスクリーンに切り替え
  // window.setFullscreen(true);

  // 表示画像を準備
  cv::Mat image(window.size().height, window.size().width,
                CV_8UC3, cv::Scalar(30, 30, 30));
  cv::putText(image,
              "Hello HighGUI",
              {40, 300},
              cv::FONT_HERSHEY_SIMPLEX,
              2.0,
              {200, 200, 255},
              3);

  // メインループ
  while (true) {
    window.present(image);

    int key = window.pollEvents(16); // ~60Hz
    if (key == 27 /* ESC */ || key == 'q' || key == 'Q') {
      window.destroy();  // 明示的に破棄
      break;
    }
  }
  return 0;
}
