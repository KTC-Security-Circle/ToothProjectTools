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
  // WindowManager から全ウィンドウのIDリストを取得
  std::vector<win::WindowId> ids;
  win_mgr_.forEach([&](win::Window& w) {
    // 可視ウィンドウのみを対象にする
    if (w.visible()) {
      ids.push_back(w.id());
    }
  });

  if (ids.empty()) return;

  // ID順にソートしておくと挙動が安定します（作成順）
  std::sort(ids.begin(), ids.end());

  // 現在のフォーカスIDの位置を探す
  auto it = std::find(ids.begin(), ids.end(), focused_id_);

  if (it == ids.end()) {
    // 現在のフォーカスが見つからない（閉じた場合など） -> 先頭へ
    focused_id_ = ids[0];
  } else {
    // 次の要素へ（末尾なら先頭へループ）
    auto next_it = std::next(it);
    if (next_it == ids.end()) {
      focused_id_ = ids[0];
    } else {
      focused_id_ = *next_it;
    }
  }

  LOG_INFO("フォーカス切り替え: New ID={}", focused_id_);
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
