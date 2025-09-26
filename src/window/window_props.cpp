// =============================================================================
//  window_props.cpp
//  ウィンドウのプロパティ操作（可視状態、フルスクリーン、移動・リサイズ）
// =============================================================================

#include "window/window.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>

// -----------------------------------------------------------------------------
namespace win {

// 可視状態
void Window::setVisible(bool is_on) noexcept {
  visible_ = is_on;
  LOG_DEBUG("可視状態を設定: name='{}', 可視={}", name_, visible_);
}

// フルスクリーン
void Window::setFullscreen(bool is_on) {
  fullscreen_ = is_on;
  cv::setWindowProperty(
      name_, cv::WND_PROP_FULLSCREEN,
      is_on ? cv::WINDOW_FULLSCREEN : cv::WINDOW_NORMAL);
  LOG_INFO("フルスクリーンを設定: name='{}', 全画面={}", name_, fullscreen_);
}

// 移動
void Window::move(Point new_position) {
  pos_ = new_position;
  if (created_) {
    cv::moveWindow(name_, pos_.x, pos_.y);
    LOG_DEBUG("ウィンドウ移動: name='{}' -> 座標({}, {})",
              name_, pos_.x, pos_.y);
  }
}

// リサイズ
void Window::resize(Size new_size) {
  size_ = new_size;
  if (created_) {
    cv::resizeWindow(name_, size_.width, size_.height);
    LOG_DEBUG("ウィンドウサイズ変更: name='{}' -> {}x{}",
              name_, size_.width, size_.height);
  }
}

} // namespace win
