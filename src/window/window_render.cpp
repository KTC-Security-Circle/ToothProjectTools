// =============================================================================
//  window_render.cpp
//  フレームの受け渡しと描画処理（二重バッファ方式）
// =============================================================================

#include "window/window.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>
#include <chrono>
#include <utility>

namespace win {

// -----------------------------------------------------------------------------
// フレームを設定（コピー版）
// -----------------------------------------------------------------------------
void Window::setImage(const cv::Mat& img) {
  back_ = img.clone();
  dirty_.store(true, std::memory_order_release);
}

// -----------------------------------------------------------------------------
// フレームを設定（ムーブ版）
// -----------------------------------------------------------------------------
void Window::setImage(cv::Mat&& img) {
  back_ = std::move(img);
  dirty_.store(true, std::memory_order_release);
}

// -----------------------------------------------------------------------------
// フレームを描画
// -----------------------------------------------------------------------------
void Window::present() {
  if (!created_) {
    LOG_WARN("present() 前に create() が必要だったため自動作成します: '{}'", name_);
    create(cv::WINDOW_NORMAL);
  }
  if (dirty_.load(std::memory_order_acquire)) {
    using std::swap;
    swap(front_, back_);
    dirty_.store(false, std::memory_order_release);
  }
  if (front_.empty()) return;

  cv::imshow(name_, front_);
  last_presented_ = std::chrono::steady_clock::now();
}

} // namespace win
