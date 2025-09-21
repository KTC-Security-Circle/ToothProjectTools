#include "window/window.hpp"
#include "window/monitor.hpp"

// =============================================================================
// Heavy includes (実装側に寄せる)
// =============================================================================
#include <opencv2/highgui.hpp>
#include <opencv2/core/mat.hpp>

#include <atomic>
#include <chrono>
#include <utility>  // std::move

namespace win {

// =============================================================================
// ID generator (thread-safe)
// =============================================================================
namespace {
  std::atomic<Window::Id> g_next_id{0};
}
Window::Id Window::nextId() { return ++g_next_id; }

// =============================================================================
// Ctor
// =============================================================================
Window::Window(std::string window_name,
               Size window_size,
               Point window_position,
               int monitor_index,
               LayoutMode layout_mode,
               bool is_visible,
               bool is_fullscreen,
               int z_index,
               int refresh_rate_hz)
  : id_(nextId()),
    name_(std::move(window_name)),
    size_(window_size),
    pos_(window_position),
    monitor_index_(monitor_index),
    layout_(layout_mode),
    visible_(is_visible),
    fullscreen_(is_fullscreen),
    z_index_(z_index),
    refresh_rate_hz_(refresh_rate_hz) {}

// =============================================================================
// Lifecycle
// =============================================================================
void Window::create(int create_flags) {
  if (created_) return;

  // デフォルト：明示しない場合は通常ウィンドウ
  if (create_flags == 0) {
    create_flags = cv::WINDOW_NORMAL;
  }

  // 1) 実ウィンドウ作成
  cv::namedWindow(name_, create_flags);
  created_ = true;

  // 2) 初期サイズ／位置
  if (create_flags == cv::WINDOW_NORMAL) {
    cv::resizeWindow(name_, size_.width, size_.height);
  }
  cv::moveWindow(name_, pos_.x, pos_.y);

  // 3) 可視／フルスクリーン
  setVisible(visible_);
  setFullscreen(fullscreen_);
}

void Window::destroy() noexcept {
  if (!created_) return;
  cv::destroyWindow(name_);
  created_ = false;
}

// =============================================================================
// Rendering
// =============================================================================
void Window::present(const cv::Mat& frame) {
  if (!created_) create(cv::WINDOW_NORMAL);
  cv::imshow(name_, frame);
  last_presented_ = std::chrono::steady_clock::now();
}

// =============================================================================
// Properties & Operations
// =============================================================================
void Window::setVisible(bool is_on) noexcept {
  visible_ = is_on;
  // HighGUI には明示的な hide/show API がないため、
  // ここでは「存在＝可視」とみなす最小実装。
  (void)is_on;
}

void Window::setFullscreen(bool is_on) {
  fullscreen_ = is_on;
  cv::setWindowProperty(
      name_,
      cv::WND_PROP_FULLSCREEN,
      is_on ? cv::WINDOW_FULLSCREEN : cv::WINDOW_NORMAL);
  // 注: マルチモニタでは主モニタに張り付く事例あり（バックエンド依存）。
}

void Window::move(Point new_position) {
  pos_ = new_position;
  if (created_) {
    cv::moveWindow(name_, pos_.x, pos_.y);
  }
}

void Window::resize(Size new_size) {
  size_ = new_size;
  if (created_) {
    cv::resizeWindow(name_, size_.width, size_.height);
  }
}

void Window::setMonitorIndex(int new_index) {
  // Xrandr で列挙
  auto monitors = enumerate_monitors_x11();

  if (monitors.empty()) {
    // 列挙できなければフォールバック（モニタ1想定で原点へ）
    monitor_index_ = 1;
    pos_ = {0, 0};
  } else {
    // API は 1 始まり。範囲外は 1 にフォールバック。
    const int clamped_index1 = (new_index >= 1 && new_index <= static_cast<int>(monitors.size()))
                                 ? new_index : 1;
    const int idx0 = clamped_index1 - 1;

    monitor_index_ = clamped_index1;
    pos_ = {monitors[idx0].x, monitors[idx0].y};
  }

  if (created_) {
    cv::moveWindow(name_, pos_.x, pos_.y);
  }
}

// =============================================================================
// Events
// =============================================================================
int Window::pollEvents(int delay_ms) {
  // HighGUI のイベントループは waitKey のみ
  return cv::waitKey(delay_ms);
}

} // namespace win
