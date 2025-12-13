// =============================================================================
//  app_core.cpp
//  コンストラクタ、メインループ、入力処理、更新、描画を担当するモジュール
// =============================================================================

#include "app/app.hpp"
#include "video/camera.hpp"
#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"
#include "structured_light/structured_light.hpp"

#include "app/app_locals.hpp"

#include <type_traits>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <utility>

// -----------------------------------------------------------------------------
// コンストラクタ (軽量化)
// -----------------------------------------------------------------------------
App::App() {
  // 基本的なメンバ変数の初期化のみ行い、
  // ウィンドウ作成などの重い処理は init() へ委譲します。
}

// -----------------------------------------------------------------------------
// 実行エントリポイント
// -----------------------------------------------------------------------------
void App::run() {
  init(); // 初期化・構築
  loop(); // メインループ開始
}

// -----------------------------------------------------------------------------
// 初期化 (構築ロジック)
// -----------------------------------------------------------------------------
void App::init() {
  LOG_INFO("App: 初期化を開始します");

  // =========================================================
  // 1. ウィンドウの生成 (Manager経由)
  // =========================================================

  // --- Window 1: Preview ---
  id_preview_ = win_mgr_.createWindow("Preview", {800, 600}, {100, 100});
  if (auto* w = win_mgr_.get(id_preview_)) {
    w->setMonitorIndex(1);
    focused_id_ = id_preview_;
  }

  // --- Window 2: Second ---
  id_second_ = win_mgr_.createWindow("Second", {800, 600}, {900, 200});
  if (auto* w = win_mgr_.get(id_second_)) {
    w->setMonitorIndex(2);
  }

  // --- Window 3: Projector ---
  id_projector_ = win_mgr_.createWindow("Projector", {0, 0}, {0, 0});
  
  int proj_w = 1920;
  int proj_h = 1080;

  if (auto* w = win_mgr_.get(id_projector_)) {
    w->setMonitorIndex(2); // モニタ2へ移動

    win::Size size = w->getMonitorSize();
    if (size.width > 0 && size.height > 0) {
      proj_w = size.width;
      proj_h = size.height;
    } else {
      LOG_WARN("Projectorサイズ取得失敗。デフォルト(1920x1080)を使用");
    }
    
    w->resize({proj_w, proj_h});
  }

  // =========================================================
  // 2. StructuredLight の初期化
  // =========================================================
  LOG_INFO("StructuredLight初期化: Projector Resolution={}x{}", proj_w, proj_h);
  
  sl_system_ = std::make_unique<sl::StructuredLight>(proj_w, proj_h);
  sl_system_->generatePatterns();

  // =========================================================
  // 3. 初期画像の適用
  // =========================================================
  if (auto* w = win_mgr_.get(id_preview_)) {
    cv::Mat img = cv::Mat(w->size().height, w->size().width, CV_8UC3, cv::Scalar(30, 30, 30));
    cv::putText(img, "Hello Preview", {40, 300}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {200, 200, 255}, 3);
    w->setImage(std::move(img));
  }

  if (auto* w = win_mgr_.get(id_second_)) {
    cv::Mat img = cv::Mat(w->size().height, w->size().width, CV_8UC3, cv::Scalar(30, 30, 30));
    cv::putText(img, "Hello Second", {40, 300}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {200, 200, 255}, 3);
    w->setImage(std::move(img));
  }

  if (auto* w = win_mgr_.get(id_projector_)) {
    cv::Mat img = cv::Mat(proj_h, proj_w, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::putText(img, "Projector Ready", {100, proj_h/2}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {255, 255, 255}, 3);
    w->setImage(std::move(img));
  }

  // =========================================================
  // 4. カメラの初期化 (Manager経由)
  // =========================================================
  
  // --- Camera 1 ---
  video::CameraOptions opt1;
  opt1.device_index = 4;
  id_cam1_ = cam_mgr_.createCamera(opt1, "Cam0");
  
  if (id_cam1_ != video::kInvalidCameraId) {
    cam_to_win_[id_cam1_] = id_preview_;
  }

  // --- Camera 2 ---
  video::CameraOptions opt2;
  opt2.device_index = 6;
  id_cam2_ = cam_mgr_.createCamera(opt2, "Cam1");
  
  if (id_cam2_ != video::kInvalidCameraId) {
    cam_to_win_[id_cam2_] = id_second_;
  }

  // =========================================================
  // 5. 表示確定とコールバック設定
  // =========================================================
  
  win_mgr_.forEach([](win::Window& w){
    w.present();
  });

  #if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
    cv::pollKey();
  #else
    cv::waitKey(1);
  #endif

  mouse_callback_contexts_.reserve(win_mgr_.count());
  
  win_mgr_.forEach([this](win::Window& w){
    mouse_callback_contexts_.push_back(MouseCallbackContext{this, static_cast<win::WindowId>(w.id())});
    
    cv::setMouseCallback(
      w.name().c_str(),
      &App::onMouseCallback,
      static_cast<void*>(&mouse_callback_contexts_.back())
    );
  });

  // キーバインド
  install_default_bindings(input_, cmd_que_);

  // 'p': パターン0を表示
  input_.bind('p', [this](){
      if (id_projector_ == win::kInvalidWindowId) return;

      cmd_que_.push_back(DispatchCmd{
          TargetById{ id_projector_ }, 
          CmdShowPattern{ 0 }
      });
  });

  // 'n': 次のパターンを表示 (★ここを修正しました)
  input_.bind('n', [this](){
      if (id_projector_ == win::kInvalidWindowId) return;

      // 現在のインデックス + 1 を計算
      int next_idx = current_pattern_index_ + 1;

      // パターン数を超えたら0に戻す（ループ）
      if (sl_system_ && next_idx >= (int)sl_system_->getPatternCount()) {
          next_idx = 0;
      }

      cmd_que_.push_back(DispatchCmd{
          TargetById{ id_projector_ },
          CmdShowPattern{ next_idx }
      });
  });
  
  LOG_INFO("App: 初期化完了");
}

// -----------------------------------------------------------------------------
// メインループ
// -----------------------------------------------------------------------------
void App::loop() {
  while (running_) {
    processInput();
    update();
    render();
  }
}

// -----------------------------------------------------------------------------
// 入力処理
// -----------------------------------------------------------------------------
void App::processInput() {
#if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
  const int pressed_key_code = cv::pollKey();
#else
  const int pressed_key_code = cv::waitKey(16);
#endif
  input_.handle(pressed_key_code);
}

// -----------------------------------------------------------------------------
// 更新処理
// -----------------------------------------------------------------------------
void App::update() {
  // 1) カメラ更新 (CameraManagerを使用)
  cam_mgr_.forEach([this](video::Camera& cam){
    if (!cam.isOpened()) return;

    cv::Mat captured_frame = cam.getFrame();
    if (captured_frame.empty()) return;

    // 紐付いているウィンドウがあれば画像を送る
    auto it = cam_to_win_.find(cam.id());
    if (it != cam_to_win_.end()) {
      win::WindowId target_id = it->second;
      if (auto* w = win_mgr_.get(target_id)) {
        w->setImage(std::move(captured_frame));
      }
    }
  });

  // 2) コマンド適用
  while (!cmd_que_.empty()) { 
    const DispatchCmd& next_command = cmd_que_.front();
    dispatch(next_command);
    cmd_que_.pop_front();
    if (!running_) break;
  }
  
  // 3) 構造光シーケンス制御 (必要に応じて実装)
  /*
  if (is_scanning_) {
      // ...
  }
  */
}

// -----------------------------------------------------------------------------
// 描画処理
// -----------------------------------------------------------------------------
void App::render() {
  if (skip_render_once_) { skip_render_once_ = false; return; }

  const auto now_steady = std::chrono::steady_clock::now();

  // WindowManager経由で全ウィンドウを描画
  win_mgr_.forEach([now_steady](win::Window& w){
    // existsCheckはManagerが生存管理しているので不要。Visibleだけ見る
    if (!w.visible()) return;

    const int refresh_rate = w.refreshRate();
    const auto min_delta = std::chrono::nanoseconds(1'000'000'000LL / std::max(1, refresh_rate));

    if (now_steady - w.lastPresented() >= min_delta) {
      w.present();
    }
  });
}
