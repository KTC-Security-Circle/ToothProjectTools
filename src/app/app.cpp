#include "app/app.hpp"
#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"

#include <type_traits>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

static inline bool existsAndVisible(const win::Window& w_ptr) {
  // WND_PROP_VISIBLE >= 0 なら「存在・可視」：存在確認に使える
  return cv::getWindowProperty(
    w_ptr.name().c_str(),
    cv::WND_PROP_VISIBLE
  ) >= 0;
}

App::App()
{
  // --- 1枚目 ---
  windows_.emplace_back("Preview", win::Size{800, 600}, win::Point{100, 100});
  win::Window& first_window = windows_.back();
  first_window.create();
  first_window.setMonitorIndex(1);
  focused_id_ = first_window.id();  // フォーカスをこのウィンドウに設定

  // 1枚目の画像
  image_ = cv::Mat(first_window.size().height, first_window.size().width,
                   CV_8UC3, cv::Scalar(30, 30, 30));
  cv::putText(image_,
              "Hello HighGUI",
              {40, 300},
              cv::FONT_HERSHEY_SIMPLEX,
              2.0,
              {200, 200, 255},
              3);

  // --- 2枚目 ---
  windows_.emplace_back("Second", win::Size{640, 480}, win::Point{900, 200});
  win::Window& second_window = windows_.back();
  second_window.create();
  second_window.setMonitorIndex(2);

  // マウスコールバックの登録
  mouse_callback_contexts_.reserve(windows_.size());
  for (auto& window_instance : windows_) {
    mouse_callback_contexts_.push_back(MouseCallbackContext{
      this,                   // App* を渡す
      window_instance.id()    // このコールバックが紐付く Window の ID
    });
    MouseCallbackContext* context_ptr = &mouse_callback_contexts_.back();

    cv::setMouseCallback(
      window_instance.name().c_str(),
      &App::onMouseCallback,        // キャプチャなしの関数ポインタ
      static_cast<void*>(context_ptr)
    );
  }

  install_default_bindings(input_, cmd_que_);
}


void App::run() {
  while (running_) {
    processInput();
    update();
    render();
  }
}

void App::processInput() {
  // HighGUI の唯一のイベント経路。周期的に呼ぶ必要あり
  // pollKey() が使えるなら非ブロッキング。環境差が気になるなら waitKey(16) でもOK。
#if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
  const int key = cv::pollKey();
#else
  const int key = cv::waitKey(16);
#endif
  input_.handle(key);
}

void App::update() {
  while (!cmd_que_.empty()) {                // ← std::deque<DispatchCmd>
    const DispatchCmd& next = cmd_que_.front();
    dispatch(next);                                 // 宛先解決＋コマンド適用
    cmd_que_.pop_front();

    if (!running_) break;                           // Quit でループ離脱
  }
}


void App::render() {
  if (skip_render_once_) { skip_render_once_ = false; return; }
  if (auto* focused_window = findWindowById(focused_id_)) {
    if (existsAndVisible(*focused_window)) {
      focused_window->present(image_);
    }
  }
}


// === アプリ全体に直接作用するコマンド ===============================
bool App::handleAppLevelCommand_(const Command& cmd) {
  bool handled = false;
  std::visit([&](auto&& c){
    using T = std::decay_t<decltype(c)>;
    if constexpr (std::is_same_v<T, CmdFocusNext>) {
      LOG_INFO("コマンド: フォーカス移動（次）");
      doFocusNext();
      handled = true;
    }
  }, cmd);
  return handled;
}

// === 1ウィンドウへコマンド適用 ======================================
void App::applyCommandToWindow_(win::Window& w, const Command& cmd) {
  std::visit([&](auto&& c){
    using T = std::decay_t<decltype(c)>;
    if constexpr (std::is_same_v<T, CmdToggleFullscreen>) {
      LOG_INFO("コマンド: フルスクリーン切替 -> Window id={}, name='{}'",
               w.id(), w.name());
      w.setFullscreen(!w.fullscreen());
    } else if constexpr (std::is_same_v<T, CmdMoveToMonitor>) {
      LOG_INFO("コマンド: モニタ移動 index={} -> Window id={}, name='{}'",
               c.index, w.id(), w.name());
      w.setMonitorIndex(c.index);
    } else if constexpr (std::is_same_v<T, CmdQuit>) {
      LOG_INFO("コマンド: 終了要求 -> アプリ全体に適用");
      running_ = false;
    }
  }, cmd);
}

// === 全ウィンドウ宛 ==================================================
void App::dispatchToAll_(const DispatchCmd& d) {
  LOG_INFO("宛先: 全ウィンドウ");
  for (auto& w : windows_) {
    if (existsAndVisible(w)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'", w.id(), w.name());
      applyCommandToWindow_(w, d.cmd);
    }
  }
}

// === フォーカス宛 ====================================================
void App::dispatchToFocused_(const DispatchCmd& d) {
  LOG_INFO("宛先: フォーカス中のウィンドウ id={}", focused_id_);
  if (auto* w = findWindowById(focused_id_)) {
    if (existsAndVisible(*w)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'", w->id(), w->name());
      applyCommandToWindow_(*w, d.cmd);
    } else {
      LOG_INFO("  フォーカス中のウィンドウは不可視または存在しません");
    }
  } else {
    LOG_INFO("  フォーカス中のウィンドウは見つかりませんでした");
  }
}

// === 指定ID宛 =======================================================
void App::dispatchToId_(const DispatchCmd& d, WindowId id) {
  LOG_INFO("宛先: 指定IDのウィンドウ id={}", id);
  if (auto* w = findWindowById(id)) {
    if (existsAndVisible(*w)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'", w->id(), w->name());
      applyCommandToWindow_(*w, d.cmd);
    } else {
      LOG_INFO("  指定IDのウィンドウは不可視または存在しません");
    }
  } else {
    LOG_INFO("  指定IDのウィンドウは見つかりませんでした");
  }
}

// === 共通の後処理（HighGUIイベントポンプ＋描画スキップ） ============
void App::finalizeDispatch_() {
  cv::waitKey(1);        // HighGUIのイベント処理は waitKey/pollKey が唯一の経路。:contentReference[oaicite:0]{index=0}
  skip_render_once_ = true;
}

// === 本体: シンプルなハブに縮小 =============================================
void App::dispatch(const DispatchCmd& d) {
  // 1) 先にアプリ全体に直接作用するものを処理
  if (handleAppLevelCommand_(d.cmd)) {
    finalizeDispatch_();
    return;
  }

  // 2) 宛先に応じてルーティング
  std::visit([&](auto&& tgt){
    using T = std::decay_t<decltype(tgt)>;
    if constexpr (std::is_same_v<T, TargetAll>) {
      dispatchToAll_(d);
    } else if constexpr (std::is_same_v<T, TargetFocused>) {
      dispatchToFocused_(d);
    } else if constexpr (std::is_same_v<T, TargetById>) {
      dispatchToId_(d, tgt.id);
    }
  }, d.target);

  // 3) 共通の後処理
  finalizeDispatch_();
}

void App::onMouseCallback(int event, int x, int y, int flags, void* userdata) {
  (void)x;
  (void)y;
  (void)flags;

  MouseCallbackContext* context_ptr = static_cast<MouseCallbackContext*>(userdata);
  if (!context_ptr) return;

  if (event == cv::EVENT_LBUTTONDOWN) {
    App* app_ptr = context_ptr->app_ptr;
    win::Window::Id clicked_id = context_ptr->window_id;

    app_ptr->focused_id_ = clicked_id;

    // ログ（変数名は省略しない）
    if (auto* focused_window_ptr = app_ptr->findWindowById(clicked_id)) {
      LOG_INFO("マウスクリックでフォーカス変更: id={}, name='{}'",
               focused_window_ptr->id(), focused_window_ptr->name());
    } else {
      LOG_WARN("マウスクリックで取得した id={} に対応するウィンドウが見つかりませんでした",
               static_cast<std::uint64_t>(clicked_id));
    }
  }
}

void App::doFocusNext() {
  if (windows_.empty()) return;
  // 現在の位置を探して次へ
  size_t i = 0;
  for (; i < windows_.size(); ++i) if (windows_[i].id() == focused_id_) break;
  focused_id_ = windows_[(i + 1) % windows_.size()].id();
}

void App::doToggleFullscreen() {
  if (auto* focused_window = findWindowById(focused_id_)) {
    focused_window->setFullscreen(!focused_window->fullscreen());
  }
}

void App::doMoveToMonitor(int index) {
  if (auto* focused_window = findWindowById(focused_id_)) {
    focused_window->setMonitorIndex(index);
  }
}
void App::doQuit() { running_ = false; }
