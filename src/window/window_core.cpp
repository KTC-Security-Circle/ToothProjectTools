// =============================================================================
//  window_core.cpp
//  ウィンドウの基礎的なライフサイクルや識別子管理を担当するモジュール
// =============================================================================

#include "window/window.hpp"
#include "window/monitor.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>
#include <chrono>
#include <atomic>
#include <utility>

namespace win {

// -----------------------------------------------------------------------------
// ID 発行（スレッド安全） - PropsでID指定がない場合のフォールバック
// -----------------------------------------------------------------------------
namespace {
  std::atomic<Window::Id> g_next_id{0};
}
Window::Id Window::nextId() { return ++g_next_id; }

// -----------------------------------------------------------------------------
// コンストラクタ (WindowProps 受け取り)
// -----------------------------------------------------------------------------
Window::Window(const WindowProps& props)
  : id_(props.id != kInvalidWindowId ? props.id : nextId()), // ID指定があれば採用
    name_(props.name),
    // ★修正: () -> {} に変更（構造体初期化対応）
    size_{props.width, props.height},
    pos_{props.x, props.y},
    monitor_index_(props.monitor_index),
    layout_(props.layout),
    visible_(props.visible),
    fullscreen_(props.fullscreen),
    z_index_(props.z_index),
    refresh_rate_hz_(props.refresh_hz) 
{
  LOG_DEBUG("ウィンドウ構築: id={}, name='{}', サイズ={}x{}, 位置=({}, {}), モニタ={}, "
            "レイアウト={}, 可視={}, 全画面={}, Z={}, リフレッシュレート={}Hz",
            id_, name_, size_.width, size_.height,
            pos_.x, pos_.y,
            monitor_index_, static_cast<int>(layout_),
            visible_, fullscreen_, z_index_, refresh_rate_hz_);
}

// -----------------------------------------------------------------------------
// ムーブコンストラクタ
// -----------------------------------------------------------------------------
Window::Window(Window&& o) noexcept
  : id_(o.id_),
    name_(std::move(o.name_)),
    size_(o.size_),
    pos_(o.pos_),
    monitor_index_(o.monitor_index_),
    layout_(o.layout_),
    visible_(o.visible_),
    fullscreen_(o.fullscreen_),
    z_index_(o.z_index_),
    refresh_rate_hz_(o.refresh_rate_hz_),
    front_(std::move(o.front_)),
    back_(std::move(o.back_)),
    dirty_(o.dirty_.exchange(false, std::memory_order_acq_rel)),
    last_presented_(o.last_presented_),
    created_(o.created_) {
  o.created_ = false;
}

// -----------------------------------------------------------------------------
// ムーブ代入演算子
// -----------------------------------------------------------------------------
Window& Window::operator=(Window&& o) noexcept {
  if (this == &o) return *this;
  
  // 破棄処理
  destroy();

  id_ = o.id_;
  name_ = std::move(o.name_);
  size_ = o.size_;
  pos_  = o.pos_;
  monitor_index_ = o.monitor_index_;
  layout_ = o.layout_;
  visible_ = o.visible_;
  fullscreen_ = o.fullscreen_;
  z_index_ = o.z_index_;
  refresh_rate_hz_ = o.refresh_rate_hz_;
  
  created_ = o.created_;
  last_presented_ = o.last_presented_;
  front_ = std::move(o.front_);
  back_  = std::move(o.back_);
  dirty_.store(o.dirty_.exchange(false, std::memory_order_acq_rel),
               std::memory_order_release);
  
  o.created_ = false;
  return *this;
}

// -----------------------------------------------------------------------------
// ウィンドウ生成
// -----------------------------------------------------------------------------
void Window::create(int create_flags) {
  if (name_.empty()) {
    LOG_ERROR("create() で name_ が空です (id={})", id_);
    return;
  }
  if (created_) {
    LOG_DEBUG("create() は既に生成済みのためスキップ: name='{}'", name_);
    return;
  }
  if (create_flags == 0) create_flags = cv::WINDOW_NORMAL;

  cv::namedWindow(name_, create_flags);
  created_ = true;
  LOG_INFO("ウィンドウを作成しました: id={}, name='{}', flags={}", id_, name_, create_flags);

  if (create_flags == cv::WINDOW_NORMAL) {
    cv::resizeWindow(name_, size_.width, size_.height);
  }
  cv::moveWindow(name_, pos_.x, pos_.y);

  // 初期プロパティの適用
  if (fullscreen_) {
      setFullscreen(true);
  }
  // 可視性制御が必要な場合はここに追加
}

// -----------------------------------------------------------------------------
// ウィンドウ破棄
// -----------------------------------------------------------------------------
void Window::destroy() noexcept {
  if (!created_) return;
  cv::destroyWindow(name_);
  created_ = false;
  LOG_INFO("ウィンドウを破棄しました: id={}, name='{}'", id_, name_);
}

// -----------------------------------------------------------------------------
// イベントポンプ
// -----------------------------------------------------------------------------
int Window::pollEvents(int delay_ms) {
  const int key = cv::waitKey(delay_ms);
  return key;
}

} // namespace win