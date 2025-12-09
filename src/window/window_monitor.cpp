// =============================================================================
//  window_monitor.cpp
//  モニタ切替処理（XRandRを用いたモニタ列挙と移動）
// =============================================================================

#include "window/window.hpp"
#include "window/monitor.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>

namespace win {

// -----------------------------------------------------------------------------
// モニタ切替
// -----------------------------------------------------------------------------
void Window::setMonitorIndex(int new_index) {
  const bool was_fullscreen = fullscreen_;
  Size was_size = size_;
  const bool can_touch_os = created_ && !name_.empty();
  if (can_touch_os && was_fullscreen) {
    setFullscreen(false);
  }

  auto monitors = enumerate_monitors_x11();
  if (monitors.empty()) {
    LOG_WARN("XRandR でモニタを列挙できません。index=1 にフォールバック");
    monitor_index_ = 1;
    pos_ = {0, 0};
  } else {
    const int clamped_index1 =
        (new_index >= 1 && new_index <= (int)monitors.size()) ? new_index : 1;
    const int index0_based = clamped_index1 - 1;
    monitor_index_ = clamped_index1;
    pos_ = {monitors[index0_based].x, monitors[index0_based].y};
    LOG_INFO("モニタ切替: name='{}', monitor={} -> 位置({}, {}) サイズ={}x{}",
             name_, monitor_index_, pos_.x, pos_.y,
             monitors[index0_based].width, monitors[index0_based].height);
  }

  if (can_touch_os) {
    cv::moveWindow(name_, pos_.x, pos_.y);
  }

  if (can_touch_os && was_fullscreen) {
    if (was_size.width > 0 && was_size.height > 0) {
      cv::resizeWindow(name_, was_size.width, was_size.height);
    }
    setFullscreen(true);
  }
}

Size Window::getMonitorSize() const {
  // モニタ情報を列挙
  auto monitors = enumerate_monitors_x11();
  
  if (monitors.empty()) {
    LOG_ERROR("getMonitorSize: モニタ列挙に失敗しました");
    return Size{0, 0};
  }

  // monitor_index_ は 1-based なので 0-based に変換
  // ※ monitor_index_ が未設定(0)の場合はデフォルトで1(index 0)を返すと安全です
  int target_index = (monitor_index_ > 0) ? monitor_index_ : 1;
  int vec_index = target_index - 1;

  if (vec_index >= 0 && vec_index < static_cast<int>(monitors.size())) {
    const auto& m = monitors[vec_index];
    // Monitor構造体の width/height を返す
    return Size{m.width, m.height};
  } else {
    LOG_WARN("getMonitorSize: monitor_index_={} は範囲外です (検出数={})", 
             monitor_index_, monitors.size());
    // フォールバックとしてプライマリモニタ(0)を返すか、エラーを返す
    if (!monitors.empty()) {
        return Size{monitors[0].width, monitors[0].height};
    }
    return Size{0, 0};
  }
}

} // namespace win
