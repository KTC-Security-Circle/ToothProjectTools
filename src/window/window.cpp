#include "window/window.hpp"
#include "window/monitor.hpp"
#include "logger/logger_macros.hpp"  // LOG_INFO/LOG_WARN/LOG_ERROR など

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
/** @brief ID generator (thread-safe)
 *
 * Window::Id をスレッド安全に単調増加で払い出す。
 * 生成順序の可観測性が必要なら、この関数の呼び出し位置を明示的に制御すること。
 */
namespace {
  std::atomic<Window::Id> g_next_id{0};
}
Window::Id Window::nextId() { return ++g_next_id; }

// =============================================================================
/** @brief Ctor
 *
 * ここでは状態の初期化のみを行い、OSリソースの確保は行わない（RAIIの分離）。
 * 実際のウィンドウ生成は create() を呼ぶまで遅延される。
 */
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
    refresh_rate_hz_(refresh_rate_hz) {
  LOG_DEBUG("Window constructed: id={}, name='{}', size={}x{}, pos=({}, {}), monitor={}, layout={}, visible={}, fullscreen={}, z={}, hz={}",
            id_, name_, size_.width, size_.height, pos_.x, pos_.y, monitor_index_, static_cast<int>(layout_),
            visible_, fullscreen_, z_index_, refresh_rate_hz_);
}

// =============================================================================
/** @brief Lifecycle: create
 *
 * 実際の HighGUI ウィンドウを作成する。既に生成済みなら何もしない。
 * - `cv::namedWindow` で OS リソースを確保
 * - `cv::resizeWindow` と `cv::moveWindow` で初期配置
 * - 可視状態とフルスクリーン状態を反映
 *
 * @param create_flags HighGUI フラグ（未指定時は `cv::WINDOW_NORMAL`）
 *
 * @note `WINDOW_AUTOSIZE` を付けると `resizeWindow` は効かない点に注意。
 */
void Window::create(int create_flags) {
  if (created_) {
    LOG_DEBUG("create() skipped: already created (name='{}')", name_);
    return;
  }

  if (create_flags == 0) {
    create_flags = cv::WINDOW_NORMAL; // デフォルト：通常ウィンドウ
  }

  // 1) 実ウィンドウ作成
  cv::namedWindow(name_, create_flags);
  created_ = true;
  LOG_INFO("Window created: name='{}', flags={}", name_, create_flags);

  // 2) 初期サイズ／位置（AUTOSIZE では resizeWindow は無効）
  if (create_flags == cv::WINDOW_NORMAL) {
    cv::resizeWindow(name_, size_.width, size_.height);
  }
  cv::moveWindow(name_, pos_.x, pos_.y);
  LOG_DEBUG("Window initialized: resize to {}x{}, move to ({}, {})",
            size_.width, size_.height, pos_.x, pos_.y);

  // 3) 可視／フルスクリーン（HighGUIは明示的な非表示APIがない）
  setVisible(visible_);
  setFullscreen(fullscreen_);
}

/** @brief Lifecycle: destroy
 *
 * HighGUI 側のウィンドウを破棄する。生成されていなければ何もしない。
 * 破棄後は OS リソースが解放され、ハンドルは無効になる。
 */
void Window::destroy() noexcept {
  if (!created_) return;
  cv::destroyWindow(name_);
  created_ = false;
  LOG_INFO("Window destroyed: name='{}'", name_);
}

// =============================================================================
/** @brief Rendering: present
 *
 * 画像フレームを現在のウィンドウへ描画し、描画時刻を更新する。
 * 必要に応じて自動で `create()` を呼び、ウィンドウを確保する。
 *
 * @param frame 描画する `cv::Mat`（型・色空間は HighGUI の既定表示に準拠）
 *
 * @note HighGUI のイベント処理は `waitKey` / `pollKey` に依存するため、
 *       別途 `pollEvents()` を周期的に呼ぶこと。描画更新もイベントループに依存する。
 */
void Window::present(const cv::Mat& frame) {
  if (!created_) {
    LOG_WARN("present() called before create(); creating window: '{}'", name_);
    create(cv::WINDOW_NORMAL);
  }
  cv::imshow(name_, frame);
  last_presented_ = std::chrono::steady_clock::now();
  SPDLOG_TRACE("presented a frame: name='{}'", name_);
}

// =============================================================================
/** @brief Properties: setVisible
 *
 * HighGUI には「非表示」APIが存在しないため、本実装では状態フラグの更新のみ行う。
 * ウィンドウの存在＝可視とみなす最小実装。
 */
void Window::setVisible(bool is_on) noexcept {
  visible_ = is_on;
  SPDLOG_DEBUG("setVisible: name='{}', visible={}", name_, visible_);
}

/** @brief Properties: setFullscreen
 *
 * `cv::setWindowProperty(WND_PROP_FULLSCREEN, ...)` を用いてフルスクリーン切替を行う。
 * 一部バックエンドではマルチモニタ環境で「主モニタに貼り付く」挙動がある点に注意。
 */
void Window::setFullscreen(bool is_on) {
  fullscreen_ = is_on;
  cv::setWindowProperty(
      name_,
      cv::WND_PROP_FULLSCREEN,
      is_on ? cv::WINDOW_FULLSCREEN : cv::WINDOW_NORMAL);
  LOG_INFO("setFullscreen: name='{}', fullscreen={}", name_, fullscreen_);
}

/** @brief Properties: move
 *
 * OS座標系でウィンドウの左上座標を移動する。生成済みのときのみ即時反映。
 * @param new_position 画面座標 (x, y)
 */
void Window::move(Point new_position) {
  pos_ = new_position;
  if (created_) {
    cv::moveWindow(name_, pos_.x, pos_.y);
    SPDLOG_DEBUG("move: name='{}' -> pos=({}, {})", name_, pos_.x, pos_.y);
  }
}

/** @brief Properties: resize
 *
 * 画像エリアのサイズを変更する（`WINDOW_AUTOSIZE` では無効）。
 * @param new_size (width, height)
 */
void Window::resize(Size new_size) {
  size_ = new_size;
  if (created_) {
    cv::resizeWindow(name_, size_.width, size_.height);
    SPDLOG_DEBUG("resize: name='{}' -> {}x{}", name_, size_.width, size_.height);
  }
}

/** @brief Properties: setMonitorIndex
 *
 * XRandR で接続モニタ矩形を列挙し、1始まりの monitor index に合わせて
 * ウィンドウの配置原点を切り替える（範囲外は 1 にフォールバック）。
 *
 * @param new_index 1 始まりのモニタ番号
 *
 * @note 列挙失敗時は単一モニタ（原点）想定へフォールバック。
 *       実際のウィンドウ移動は生成済みの時のみ反映。
 */
void Window::setMonitorIndex(int new_index) {
  auto monitors = enumerate_monitors_x11();

  if (monitors.empty()) {
    LOG_WARN("No monitors enumerated by XRandR; fallback to index=1 at (0,0)");
    monitor_index_ = 1;
    pos_ = {0, 0};
  } else {
    const int clamped_index1 =
        (new_index >= 1 && new_index <= static_cast<int>(monitors.size()))
        ? new_index : 1;
    const int index0_based = clamped_index1 - 1;

    monitor_index_ = clamped_index1;
    pos_ = {monitors[index0_based].x, monitors[index0_based].y};

    LOG_INFO("setMonitorIndex: name='{}', monitor={} -> pos=({}, {}) size={}x{}",
             name_, monitor_index_, pos_.x, pos_.y,
             monitors[index0_based].width, monitors[index0_based].height);
  }

  if (created_) {
    cv::moveWindow(name_, pos_.x, pos_.y);
  }
}

// =============================================================================
/** @brief Events: pollEvents
 *
 * HighGUI のイベント処理は `waitKey` / `pollKey` に依存するため、
 * 本関数を周期的（描画周期など）に呼び出すこと。
 *
 * @param delay_ms 待機ミリ秒。0 は「無限待ち」、>0 は最短でもその程度待つ。
 * @return 押下キーコード（未入力は -1）。複数ウィンドウがある場合、いずれかがアクティブなら取得可能。
 *
 * @see OpenCV HighGUI docs: waitKey/pollKey の注意事項
 */
int Window::pollEvents(int delay_ms) {
  const int key = cv::waitKey(delay_ms);
  SPDLOG_TRACE("pollEvents: name='{}', delay={}ms -> key={}", name_, delay_ms, key);
  return key;
}

} // namespace win
