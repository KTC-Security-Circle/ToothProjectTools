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
  LOG_DEBUG("ウィンドウ構築: id={}, name='{}', サイズ={}x{}, 位置=({}, {}), モニタ={}, レイアウト={}, 可視={}, 全画面={}, Z={}, リフレッシュレート={}Hz",
            id_, name_, size_.width, size_.height, pos_.x, pos_.y, monitor_index_, static_cast<int>(layout_),
            visible_, fullscreen_, z_index_, refresh_rate_hz_);
}

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
    created_(o.created_),
    last_presented_(o.last_presented_),
    front_(std::move(o.front_)),
    back_(std::move(o.back_)),
    dirty_(o.dirty_.exchange(false, std::memory_order_acq_rel))
{
  // OSリソース所有権の扱いを明確化：move 後、元オブジェクトは生成済みフラグを落とす
  o.created_ = false;
}

Window& Window::operator=(Window&& o) noexcept {
  if (this == &o) return *this;
  // すでに生成済みならここで destroy() する設計も可
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
  dirty_.store(o.dirty_.exchange(false, std::memory_order_acq_rel), std::memory_order_release);
  o.created_ = false;
  return *this;
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
  if (name_.empty()) {
    LOG_ERROR("create() で name_ が空");
    return;
  }
  if (created_) {
    LOG_DEBUG("create() は既に生成済みのためスキップ: name='{}'", name_);
    return;
  }

  if (create_flags == 0) {
    create_flags = cv::WINDOW_NORMAL; // デフォルト：通常ウィンドウ
  }

  // 1) 実ウィンドウ作成
  cv::namedWindow(name_, create_flags);
  created_ = true;
  LOG_INFO("ウィンドウを作成しました: name='{}', flags={}", name_, create_flags);

  // 2) 初期サイズ／位置（AUTOSIZE では resizeWindow は無効）
  if (create_flags == cv::WINDOW_NORMAL) {
    cv::resizeWindow(name_, size_.width, size_.height);
  }
  cv::moveWindow(name_, pos_.x, pos_.y);
  LOG_DEBUG("ウィンドウ初期化: リサイズ {}x{}・移動 ({}, {})",
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
  LOG_INFO("ウィンドウを破棄しました: name='{}'", name_);
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
    if (name_.empty()) {
      LOG_ERROR("present() で name_ が空のため create を実施できません");
      return;
    }
    LOG_WARN("present() が create() より先に呼ばれたため、ウィンドウを作成します: '{}'", name_);
    create(cv::WINDOW_NORMAL);
  }
  // 与えられたフレームが非空なら、ウィンドウ側で所有コピーして保持
  if (!frame.empty()) {
    // Mat は参照カウントで別バッファを共有するため、寿命・改変の影響を避けるなら clone() が安全。
    // ここでは「Window が描画に使う最新フレーム」を安定保持したいので clone() します。
    current_image_ = frame.clone();
  }

  if (current_image_.empty()) {
    LOG_WARN("present(): 表示可能な画像がありません（current_image_ が空）: name='{}'", name_);
    return;
  }

  cv::imshow(name_, current_image_);
   last_presented_ = std::chrono::steady_clock::now();
   SPDLOG_TRACE("フレームを描画しました: name='{}'", name_);
 }
 
void Window::present() {
  if (!created_) {
    LOG_WARN("present() 前に create() が必要だったため自動作成します: '{}'", name_);
    create(cv::WINDOW_NORMAL);
  }

  // 新フレームがあれば front/back を入替（最新勝ち）
  if (dirty_.load(std::memory_order_acquire)) {
    // std::lock_guard<std::mutex> lk(img_mtx_);
    using std::swap;
    swap(front_, back_);
    dirty_.store(false, std::memory_order_release);
  }

  if (front_.empty()) {
    // まだ画像が来ていない/破棄済み
    return;
  }

  cv::imshow(name_, front_);
  last_presented_ = std::chrono::steady_clock::now();
}

// =============================================================================
/** @brief Properties: setVisible
 *
 * HighGUI には「非表示」APIが存在しないため、本実装では状態フラグの更新のみ行う。
 * ウィンドウの存在＝可視とみなす最小実装。
 */
void Window::setVisible(bool is_on) noexcept {
  visible_ = is_on;
  SPDLOG_DEBUG("可視状態を設定: name='{}', 可視={}", name_, visible_);
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
  LOG_INFO("フルスクリーンを設定: name='{}', 全画面={}", name_, fullscreen_);
}

void Window::setImage(const cv::Mat& img) {
  // std::lock_guard<std::mutex> lk(img_mtx_); // ロック版にするなら有効化
  back_ = img.clone();                // 所有コピー（安全）
  dirty_.store(true, std::memory_order_release);
}

void Window::setImage(cv::Mat&& img) {
  // std::lock_guard<std::mutex> lk(img_mtx_);
  back_ = std::move(img);             // 無駄なコピー回避（呼び出し側が所有権移譲）
  dirty_.store(true, std::memory_order_release);
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
    SPDLOG_DEBUG("ウィンドウ移動: name='{}' -> 座標({}, {})", name_, pos_.x, pos_.y);
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
    SPDLOG_DEBUG("ウィンドウサイズ変更: name='{}' -> {}x{}", name_, size_.width, size_.height);
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
    LOG_WARN("XRandR でモニタを列挙できませんでした。index=1・座標(0,0) にフォールバックします");
    monitor_index_ = 1;
    pos_ = {0, 0};
  } else {
    const int clamped_index1 =
        (new_index >= 1 && new_index <= static_cast<int>(monitors.size()))
        ? new_index : 1;
    const int index0_based = clamped_index1 - 1;

    monitor_index_ = clamped_index1;
    pos_ = {monitors[index0_based].x, monitors[index0_based].y};

    LOG_INFO("モニタ切替: name='{}', monitor={} -> 位置({}, {}) サイズ={}x{}",
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
  SPDLOG_TRACE("pollEvents: name='{}', 待機={}ms -> key={}", name_, delay_ms, key);
  return key;
}

} // namespace win
