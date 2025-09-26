#include "app/app.hpp"
#include "app/camera.hpp"
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
  ) > 0;
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
  cv::Mat image1 = cv::Mat(first_window.size().height, first_window.size().width,
                   CV_8UC3, cv::Scalar(30, 30, 30));
  cv::putText(image1,
              "Hello HighGUI Preview",
              {40, 300},
              cv::FONT_HERSHEY_SIMPLEX,
              2.0,
              {200, 200, 255},
              3);
  // 初期フレームをウィンドウに保持させる
  first_window.setImage(std::move(image1));

  // Camera( index=0, id=1 )
  cameras_.emplace_back(/*index*/0, /*id*/1, "Cam0");
  if (!cameras_.back().open()) {
    LOG_ERROR("Camera open failed: index=0 (接続されていない可能性)");
  }
  cam_to_win_[1] = first_window.id();

  // --- 2枚目 ---
  windows_.emplace_back("Second", win::Size{800, 600}, win::Point{900, 200});
  win::Window& second_window = windows_.back();
  second_window.create();
  second_window.setMonitorIndex(1);

  // 2枚目の画像
  cv::Mat image2 = cv::Mat(second_window.size().height, second_window.size().width,
                   CV_8UC3, cv::Scalar(30, 30, 30));
  cv::putText(image2,
              "Hello HighGUI Second",
              {40, 300},
              cv::FONT_HERSHEY_SIMPLEX,
              2.0,
              {200, 200, 255},
              3);

  // 2枚目も同じ初期フレームを保持（必要なら別画像に差し替え可）
  second_window.setImage(std::move(image2));

  cameras_.emplace_back(/*index*/4, /*id*/2, "Cam1");
  if (!cameras_.back().open()) {
    LOG_ERROR("Camera open failed: index=4 (接続されていない可能性)");
  } else {
    cam_to_win_[2] = second_window.id();
  }

  // 初回表示を確定（HighGUIはwaitKey/pollKey経由で更新されるため）
  first_window.present();
  second_window.present();
  #if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
    cv::pollKey();
  #else
    cv::waitKey(1);
  #endif

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
  // 先にカメラ更新
  for (auto& cam : cameras_) {
    if (!cam.isOpened()) continue;
    cv::Mat frame = cam.getFrame();           // 空なら前回据え置き
    if (frame.empty()) continue;

    // 対応する Window に setImage
    auto it = cam_to_win_.find(cam.id());
    if (it != cam_to_win_.end()) {
      if (auto* w = findWindowById(it->second)) {
        w->setImage(std::move(frame));        // 二重バッファの back_ に書く
      }
    }
  }

  while (!cmd_que_.empty()) {                // ← std::deque<DispatchCmd>
    const DispatchCmd& next = cmd_que_.front();
    dispatch(next);                                 // 宛先解決＋コマンド適用
    cmd_que_.pop_front();

    if (!running_) break;                           // Quit でループ離脱
  }
}


void App::render() {
  if (skip_render_once_) { skip_render_once_ = false; return; }
  const auto now = std::chrono::steady_clock::now();
  for (auto& win : windows_) {
    if (!existsAndVisible(win)) continue;
    const int refresh_hz = win.refreshRate();
    const auto min_dt = std::chrono::nanoseconds(1'000'000'000LL / std::max(1, refresh_hz));
    if (now - win.lastPresented() >= min_dt) {
      win.present(); // dirtyならswap→imshow
    }
 }
}


// === アプリ全体に直接作用するコマンド ===============================
bool App::handleAppLevelCommand_(const Command& command) {
  bool handled = false;
  std::visit([&](auto&& concrete_command){
    using ConcreteCommandType = std::decay_t<decltype(concrete_command)>;
    if constexpr (std::is_same_v<ConcreteCommandType, CmdFocusNext>) {
      LOG_INFO("コマンド: フォーカス移動（次）");
      doFocusNext();
      handled = true;
    }
  }, command);
  return handled;
}

// === 1ウィンドウへコマンド適用 ======================================
void App::applyCommandToWindow_(win::Window& target_window, const Command& command) {
  std::visit([&](auto&& concrete_command){
    using ConcreteCommandType = std::decay_t<decltype(concrete_command)>;
    if constexpr (std::is_same_v<ConcreteCommandType, CmdToggleFullscreen>) {
      LOG_INFO("コマンド: フルスクリーン切替 -> Window id={}, name='{}'",
               target_window.id(), target_window.name());
      target_window.setFullscreen(!target_window.fullscreen());
    } else if constexpr (std::is_same_v<ConcreteCommandType, CmdMoveToMonitor>) {
      LOG_INFO("コマンド: モニタ移動 index={} -> Window id={}, name='{}'",
               concrete_command.index, target_window.id(), target_window.name());
      target_window.setMonitorIndex(concrete_command.index);
    } else if constexpr (std::is_same_v<ConcreteCommandType, CmdQuit>) {
      LOG_INFO("コマンド: 終了要求 -> アプリ全体に適用");
      running_ = false;
    }
  }, command);
}

// === 全ウィンドウ宛 ==================================================
void App::dispatchToAll_(const DispatchCmd& dispatch_command) {
  LOG_INFO("宛先: 全ウィンドウ");
  for (auto& window_instance : windows_) {
    if (existsAndVisible(window_instance)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'",
               window_instance.id(), window_instance.name());
      applyCommandToWindow_(window_instance, dispatch_command.cmd);
    }
  }
}

// === フォーカス宛 ====================================================
void App::dispatchToFocused_(const DispatchCmd& dispatch_command) {
  LOG_INFO("宛先: フォーカス中のウィンドウ id={}", focused_id_);
  if (auto* focused_window = findWindowById(focused_id_)) {
    if (existsAndVisible(*focused_window)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'",
               focused_window->id(), focused_window->name());
      applyCommandToWindow_(*focused_window, dispatch_command.cmd);
    } else {
      LOG_INFO("  フォーカス中のウィンドウは不可視または存在しません");
    }
  } else {
    LOG_INFO("  フォーカス中のウィンドウは見つかりませんでした");
  }
}

// === 指定ID宛 =======================================================
void App::dispatchToId_(const DispatchCmd& dispatch_command, WindowId target_window_id) {
  LOG_INFO("宛先: 指定IDのウィンドウ id={}", target_window_id);
  if (auto* target_window = findWindowById(target_window_id)) {
    if (existsAndVisible(*target_window)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'",
               target_window->id(), target_window->name());
      applyCommandToWindow_(*target_window, dispatch_command.cmd);
    } else {
      LOG_INFO("  指定IDのウィンドウは不可視または存在しません");
    }
  } else {
    LOG_INFO("  指定IDのウィンドウは見つかりませんでした");
  }
}

// === 共通の後処理（HighGUIイベントポンプ＋描画スキップ） ============
void App::finalizeDispatch_() {
  skip_render_once_ = true;
}

// === 本体: シンプルなハブに縮小 =============================================
void App::dispatch(const DispatchCmd& dispatch_command) {
  // 1) 先にアプリ全体に直接作用するものを処理
  if (handleAppLevelCommand_(dispatch_command.cmd)) {
    finalizeDispatch_();
    return;
  }

  // 2) 宛先に応じてルーティング
  std::visit([&](auto&& target_variant){
    using TargetType = std::decay_t<decltype(target_variant)>;
    if constexpr (std::is_same_v<TargetType, TargetAll>) {
      dispatchToAll_(dispatch_command);
    } else if constexpr (std::is_same_v<TargetType, TargetFocused>) {
      dispatchToFocused_(dispatch_command);
    } else if constexpr (std::is_same_v<TargetType, TargetById>) {
      dispatchToId_(dispatch_command, target_variant.id);
    }
  }, dispatch_command.target);

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
