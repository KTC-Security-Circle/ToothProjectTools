// =============================================================================
//  app_mouse.cpp
//  マウスコールバックとシンプルな操作関数群を担当するモジュール
// =============================================================================

#include "app/app.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>

// -----------------------------------------------------------------------------
/** @brief マウスコールバック
 *
 * 左クリックされたウィンドウにフォーカスを移します。
 * userdata には App と Window の対応を示すコンテキスト構造体を渡します。
 */
void App::onMouseCallback(int event, int x, int y, int flags, void* userdata) {
  (void)x;
  (void)y;
  (void)flags;

  MouseCallbackContext* context_ptr = static_cast<MouseCallbackContext*>(userdata);
  if (!context_ptr) return;

  if (event == cv::EVENT_LBUTTONDOWN) {
    App* app_ptr = context_ptr->app_ptr;
    win::Window::Id clicked_window_id = context_ptr->window_id;

    app_ptr->focused_id_ = clicked_window_id;

    if (auto* focused_window_ptr = app_ptr->findWindowById(clicked_window_id)) {
      LOG_INFO("マウスクリックでフォーカス変更: id={}, name='{}'",
               focused_window_ptr->id(), focused_window_ptr->name());
    } else {
      LOG_WARN("マウスクリックで取得した id={} に対応するウィンドウが見つかりませんでした",
               static_cast<std::uint64_t>(clicked_window_id));
    }
  }
}

// -----------------------------------------------------------------------------
/** @brief フォーカスを次のウィンドウへ移す
 *
 * 現在フォーカスしているウィンドウを基準に、配列順で次のウィンドウへ
 * 巡回させます。
 */
void App::doFocusNext() {
  if (windows_.empty()) return;
  size_t index_found = 0;
  for (; index_found < windows_.size(); ++index_found) {
    if (windows_[index_found].id() == focused_id_) break;
  }
  focused_id_ = windows_[(index_found + 1) % windows_.size()].id();
}

// -----------------------------------------------------------------------------
/** @brief フルスクリーンのトグル
 *
 * フォーカス中のウィンドウがあれば、その全画面状態を反転します。
 */
void App::doToggleFullscreen() {
  if (auto* focused_window_ptr = findWindowById(focused_id_)) {
    focused_window_ptr->setFullscreen(!focused_window_ptr->fullscreen());
  }
}

// -----------------------------------------------------------------------------
/** @brief モニタ移動
 *
 * フォーカス中のウィンドウがあれば、指定インデックスのモニタへ移します。
 * setMonitorIndex() は全画面時の安全な移動を内部で担保します。
 */
void App::doMoveToMonitor(int monitor_index) {
  if (auto* focused_window_ptr = findWindowById(focused_id_)) {
    focused_window_ptr->setMonitorIndex(monitor_index);
  }
}

// -----------------------------------------------------------------------------
/** @brief アプリ終了要求
 *
 * メインループの継続条件である running_ を false に設定します。
 */
void App::doQuit() {
  running_ = false;
}
