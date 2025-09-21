// src/main.cpp
// #include "logger/logger_setup.hpp"
// #include <spdlog/spdlog.h>

// int main() {
//   public_logger::init();                // ← 一度だけ
//   SPDLOG_INFO("server start port={}", 8080);   // マクロ（最短）
//   spdlog::warn("slow request={}ms", 512);      // 関数でもOK
// }

#include "window/window.hpp"
#include <opencv2/imgproc.hpp>

int main() {
  win::Window w{"Preview", {800, 600}, {100,100}};
  w.create(cv::WINDOW_NORMAL);

  cv::Mat img(w.size().height, w.size().width, CV_8UC3, cv::Scalar(30,30,30));
  cv::putText(img, "Hello HighGUI", {40, 300}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {200,200,255}, 3);

  while (true) {
    w.present(img);
    int k = win::Window::pollEvents(16); // ~60Hz
    if (k == 27) break; // ESC で終了
  }
  return 0;
}
