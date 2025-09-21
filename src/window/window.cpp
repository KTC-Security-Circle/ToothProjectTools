#include "window/window.hpp"
#include <opencv2/highgui.hpp>

namespace win {

static std::atomic<Window::Id> g_id{0};

Window::Id Window::nextId() { return ++g_id; }

Window::Window(std::string name, Size size, Point pos, int monitor_index,
               LayoutMode layout, bool visible, bool fullscreen,
               int z_index, int refresh_hz)
  : id_(nextId()),
    name_(std::move(name)),
    size_(size),
    pos_(pos),
    monitor_index_(monitor_index),
    layout_(layout),
    visible_(visible),
    fullscreen_(fullscreen),
    z_index_(z_index),
    refresh_rate_hz_(refresh_hz) {}

void Window::create(int flags) {
  if (created_) return;

  // 1) 作成
  cv::namedWindow(name_, flags);                                   // :contentReference[oaicite:2]{index=2}
  created_ = true;

  // 2) サイズ/位置
  if (flags == cv::WINDOW_NORMAL) {
    cv::resizeWindow(name_, size_.width, size_.height);             // :contentReference[oaicite:3]{index=3}
  }
  cv::moveWindow(name_, pos_.x, pos_.y);                            // :contentReference[oaicite:4]{index=4}

  // 3) 表示・フルスクリーン
  setVisible(visible_);
  setFullscreen(fullscreen_);
}

void Window::destroy() {
  if (!created_) return;
  cv::destroyWindow(name_);
  created_ = false;
}

void Window::present(const cv::Mat& frame) {
  if (!created_) create(cv::WINDOW_NORMAL);
  cv::imshow(name_, frame);                                         // :contentReference[oaicite:5]{index=5}
  last_presented_ = std::chrono::steady_clock::now();
}

void Window::setVisible(bool on) {
  visible_ = on;
  // HighGUI: WND_PROP_VISIBLE は get には使えるが「明示的な hide/show API はない」ため、
  // 疑似的に可視/不可視を再現：不可視なら最小化 or 画面外移動等で対処する設計もあり。
  // ここでは最小実装として "存在＝可視" とみなす。
  (void)on;
}

void Window::setFullscreen(bool on) {
  fullscreen_ = on;
  cv::setWindowProperty(                                            // :contentReference[oaicite:6]{index=6}
      name_, cv::WND_PROP_FULLSCREEN,
      on ? cv::WINDOW_FULLSCREEN : cv::WINDOW_NORMAL);
  // 注意: マルチモニタ環境では主モニタに出ることが多い（制約）。:contentReference[oaicite:7]{index=7}
}

void Window::move(Point p) {
  pos_ = p;
  cv::moveWindow(name_, pos_.x, pos_.y);                            // :contentReference[oaicite:8]{index=8}
}

void Window::resize(Size s) {
  size_ = s;
  cv::resizeWindow(name_, size_.width, size_.height);               // :contentReference[oaicite:9]{index=9}
}

int Window::pollEvents(int delay_ms) {
  // HighGUI のイベント取得は waitKey のみ。定期的に呼ぶ必要あり。:contentReference[oaicite:10]{index=10}
  return cv::waitKey(delay_ms);
}

} // namespace win
