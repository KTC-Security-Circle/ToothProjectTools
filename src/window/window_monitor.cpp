// =============================================================================
//  window_monitor.cpp
//  モニタ切替処理（XRandRを用いたモニタ列挙と移動）
// =============================================================================

#include "window/window.hpp"
#include "window/monitor.hpp" // monitor.hpp で get_monitor_rect が宣言されている前提
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>
#include <algorithm> // sort用

namespace win {

// -----------------------------------------------------------------------------
// モニタ切替
// -----------------------------------------------------------------------------
void Window::setMonitorIndex(int new_index) {
  // 変更がない場合は何もしない（ただし初回作成時はチェックが必要かも）
  // if (monitor_index_ == new_index) return;

  const bool was_fullscreen = fullscreen_;
  Size was_size = size_;
  const bool can_touch_os = created_ && !name_.empty();

  if (can_touch_os && was_fullscreen) {
    setFullscreen(false);
  }

  // ★修正1: メンバ変数を更新するだけにする
  // pos_ (ローカル座標) は書き換えてはいけません！
  // {0,0} のままにしておけば、move() が適切にオフセットを加算します。
  monitor_index_ = new_index;
  
  // ログ出力: 座標計算は get_monitor_rect に任せるのでここでは簡易表示
  LOG_INFO("モニタ切替: name='{}', monitor={} -> (適用待機)", name_, monitor_index_);

  if (can_touch_os) {
    // ★修正2: 自前で cv::moveWindow せず、Window::move に委譲する
    // move() 内部で get_monitor_rect(monitor_index_) が呼ばれ、
    // 正しいオフセット(1920など) + pos_(0) が計算されて移動します。
    move(pos_);
  }

  if (can_touch_os && was_fullscreen) {
    // サイズ復元が必要な場合
    if (was_size.width > 0 && was_size.height > 0) {
      cv::resizeWindow(name_, was_size.width, was_size.height);
    }
    setFullscreen(true);
  }
}

// -----------------------------------------------------------------------------
// モニタサイズ取得
// -----------------------------------------------------------------------------
Size Window::getMonitorSize() const {
  // ★修正3: 生の列挙関数ではなく、共通のヘルパーを使う
  // これによりソート順が統一されます
  int target_index = (monitor_index_ > 0) ? monitor_index_ : 1;
  
  auto rect_opt = get_monitor_rect(target_index);
  
  if (rect_opt) {
      return Size{rect_opt->width, rect_opt->height};
  } else {
      LOG_WARN("getMonitorSize: monitor_index_={} 情報取得失敗", monitor_index_);
      return Size{0, 0};
  }
}

// -----------------------------------------------------------------------------
// モニタ矩形取得 (ヘルパー実装)
// -----------------------------------------------------------------------------
std::optional<MonitorRect> get_monitor_rect(int monitor_index) {
  // 1. OSから全モニタ情報を取得
  auto monitors = enumerate_monitors_x11();

  if (monitors.empty()) {
    LOG_WARN("モニタ情報が取得できませんでした。");
    return std::nullopt;
  }

  // 2. X座標が小さい順（左にある順）にソート
  std::sort(monitors.begin(), monitors.end(), [](const MonitorRect& a, const MonitorRect& b) {
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
  });

  // 3. インデックス境界チェック (1-based index -> 0-based index)
  int idx = monitor_index - 1;
  
  // 範囲外ならメインモニタ(0)へフォールバックしつつ警告
  if (idx < 0 || idx >= static_cast<int>(monitors.size())) {
    LOG_WARN("指定されたモニタ番号 {} は無効です (検出数: {}) -> Main(1)を使用", 
             monitor_index, monitors.size());
    if (!monitors.empty()) {
        return monitors[0];
    }
    return std::nullopt;
  }

  return monitors[idx];
}

} // namespace win