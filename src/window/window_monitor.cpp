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

} // namespace win
