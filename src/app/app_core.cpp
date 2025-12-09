// =============================================================================
//  app_core.cpp
//  コンストラクタ、メインループ、入力処理、更新、描画を担当するモジュール
// =============================================================================

#include "app/app.hpp"
#include "video/camera.hpp"
#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"
#include "structured_light/structured_light.hpp"

#include "app/app_locals.hpp"              // 内部ヘルパ（existsAndVisible）

#include <type_traits>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <utility>

App::App()
{
  // ---------------------------------------------------------
  // 1. メモリ確保 (重要)
  // ---------------------------------------------------------
  // emplace_back による再確保で参照が無効になるのを防ぐため、
  // 事前に必要なサイズを予約します。
  windows_.reserve(3);
  cameras_.reserve(2);

  // ---------------------------------------------------------
  // 2. ウィンドウの生成 (UIの構築)
  // ---------------------------------------------------------
  
  // --- Window 1 (Preview) ---
  windows_.emplace_back("Preview", win::Size{800, 600}, win::Point{100, 100});
  win::Window& win1 = windows_[0]; // vector[0] への参照
  win1.create();
  win1.setMonitorIndex(1);
  win1.setCameraId(1); // ここでセットしたIDが消えないよう reserve が必須
  focused_id_ = win1.id();

  // --- Window 2 (Second) ---
  windows_.emplace_back("Second", win::Size{800, 600}, win::Point{900, 200});
  win::Window& win2 = windows_[1]; // vector[1] への参照
  win2.create();
  win2.setMonitorIndex(2);
  win2.setCameraId(2);

  // --- Window 3: Projector ---
  // とりあえずサイズ0で初期化し、create後にモニタ情報を確定させる
  windows_.emplace_back("Projector", win::Size{0, 0}, win::Point{0, 0});
  win::Window& win_projector = windows_[2];
  win_projector.create();
  win_projector.setMonitorIndex(2); // ここでモニタ2へ移動

  //  Windowクラス経由でモニタの実際の解像度を取得
  win::Size proj_size = win_projector.getMonitorSize();
  
  // 取得できなかった場合のフォールバック (例: 1920x1080)
  if (proj_size.width == 0 || proj_size.height == 0) {
      LOG_WARN("プロジェクタサイズ取得失敗。デフォルト値(1920x1080)を使用します");
      proj_size = {1920, 1080};
  }

  // ウィンドウ自体のサイズもモニタに合わせる（フルスクリーン準備）
  win_projector.resize(proj_size); 

  // ---------------------------------------------------------
  // 3. StructuredLight の初期化 & パターン生成
  // ---------------------------------------------------------
  LOG_INFO("StructuredLight初期化: Projector Resolution={}x{}", proj_size.width, proj_size.height);

  // 取得したサイズで初期化
  sl_system_ = std::make_unique<sl::StructuredLight>(proj_size.width, proj_size.height);
  sl_system_->generatePatterns();

  // ---------------------------------------------------------
  // 3. 初期画像の適用
  // ---------------------------------------------------------
  {
    cv::Mat img1 = cv::Mat(win1.size().height, win1.size().width, CV_8UC3, cv::Scalar(30, 30, 30));
    cv::putText(img1, "Hello HighGUI Preview", {40, 300}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {200, 200, 255}, 3);
    win1.setImage(std::move(img1));

    cv::Mat img2 = cv::Mat(win2.size().height, win2.size().width, CV_8UC3, cv::Scalar(30, 30, 30));
    cv::putText(img2, "Hello HighGUI Second", {40, 300}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {200, 200, 255}, 3);
    win2.setImage(std::move(img2));

    cv::Mat img_projector = cv::Mat(win_projector.size().height, win_projector.size().width, CV_8UC3, cv::Scalar(30, 30, 30));
    cv::putText(img_projector, "Hello HighGUI Second", {40, 300}, cv::FONT_HERSHEY_SIMPLEX, 2.0, {200, 200, 255}, 3);
    win_projector.setImage(std::move(img_projector));
  }

  // ---------------------------------------------------------
  // 4. カメラの初期化 (Hardware Setup)
  // ---------------------------------------------------------

  // --- Camera 1 (Index=0, ID=1) ---
  cameras_.emplace_back(/*index*/0, /*id*/1, "Cam0");
  if (!cameras_.back().open()) {
    LOG_ERROR("Camera 1 open failed: index=0 (接続確認: /dev/video0)");
  }
  cam_to_win_[1] = win1.id(); // IDマップ登録

  // --- Camera 2 (Index=1, ID=2) ---
  // ※ログで index=0 のオープンエラーが出ているため、ここは 1 に変更すべきです
  cameras_.emplace_back(/*index*/2, /*id*/2, "Cam1"); 
  if (!cameras_.back().open()) {
    LOG_ERROR("Camera 2 open failed: index=1 (接続確認: /dev/video1)");
  } else {
    cam_to_win_[2] = win2.id(); // 成功時のみマップ登録する場合
  }

  // ---------------------------------------------------------
  // 5. 表示確定と入力バインド
  // ---------------------------------------------------------
  
  // 初回表示
  win1.present();
  win2.present();
  win_projector.present();

  #if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
    cv::pollKey();
  #else
    cv::waitKey(1);
  #endif

  // マウスコールバック
  mouse_callback_contexts_.reserve(windows_.size());
  for (auto& w : windows_) {
    mouse_callback_contexts_.push_back(MouseCallbackContext{this, w.id()});
    
    cv::setMouseCallback(
      w.name().c_str(),
      &App::onMouseCallback,
      static_cast<void*>(&mouse_callback_contexts_.back())
    );
  }

  // キーバインド
  install_default_bindings(input_, cmd_que_);
}

// -----------------------------------------------------------------------------
/** @brief メインループを回す
 *
 * run() はアプリの寿命の間、入力→更新→描画の順に呼び出します。
 * ループは doQuit() で running_ が false になるまで継続します。
 */
void App::run() {
  while (running_) {
    processInput();
    update();
    render();
  }
}

// -----------------------------------------------------------------------------
/** @brief 入力処理
 *
 * HighGUI のイベント処理は waitKey/pollKey だけが経路です。
 * ここでは非ブロッキングの pollKey が有効な場合はそれを使い、
 * そうでなければ短い待ち時間で waitKey を呼びます。
 * 得られたキーコードは input_ に渡してコマンドへ変換されます。
 */
void App::processInput() {
#if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
  const int pressed_key_code = cv::pollKey();
#else
  const int pressed_key_code = cv::waitKey(16);
#endif
  input_.handle(pressed_key_code);
}

// -----------------------------------------------------------------------------
/** @brief 更新処理
 *
 * - カメラからフレームを取得し、対応するウィンドウへ setImage します。
 * - コマンドキューに溜まった操作を順に適用します。
 */
void App::update() {
  // 1) カメラ更新
  for (auto& camera_instance : cameras_) {
    if (!camera_instance.isOpened()) continue;
    cv::Mat captured_frame = camera_instance.getFrame();  // 空なら前回据え置き
    if (captured_frame.empty()) continue;

    // 対応する Window に setImage
    auto iterator_found = cam_to_win_.find(camera_instance.id());
    if (iterator_found != cam_to_win_.end()) {
      if (auto* target_window_ptr = findWindowById(iterator_found->second)) {
        target_window_ptr->setImage(std::move(captured_frame)); // 二重バッファ back_ に書く
      }
    }
  }

  // 2) コマンド適用
  while (!cmd_que_.empty()) { // ← std::deque<DispatchCmd>
    const DispatchCmd& next_command = cmd_que_.front();
    dispatch(next_command);   // 宛先解決＋コマンド適用
    cmd_que_.pop_front();
    if (!running_) break;     // Quit でループ離脱
  }
}

// -----------------------------------------------------------------------------
/** @brief 描画処理
 *
 * 各ウィンドウのリフレッシュレートに合わせて present() を呼び、
 * 二重バッファを入れ替えて表示を更新します。HighGUI の実描画は
 * pollKey/waitKey に依存するため、呼び出し側で定期的に processInput()
 * を回している前提です。
 */
void App::render() {
  if (skip_render_once_) { skip_render_once_ = false; return; }

  const auto now_steady = std::chrono::steady_clock::now();
  for (auto& window_instance : windows_) {
    if (!existsAndVisible(window_instance)) continue;

    const int refresh_rate_hz_value = window_instance.refreshRate();
    const auto minimum_delta = std::chrono::nanoseconds(
        1'000'000'000LL / std::max(1, refresh_rate_hz_value));

    if (now_steady - window_instance.lastPresented() >= minimum_delta) {
      window_instance.present(); // dirtyならswap→imshow
    }
  }
}
