// =============================================================================
//  app_core.cpp
//  コンストラクタ、メインループ、入力処理、更新、描画を担当するモジュール
// =============================================================================

#include "app/app.hpp"
#include "video/camera.hpp"
#include "input/bindings_default.hpp"
#include "logger/logger_macros.hpp"

#include "app/app_locals.hpp"              // 内部ヘルパ（existsAndVisible）

#include <type_traits>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <utility>

// -----------------------------------------------------------------------------
/** @brief コンストラクタ
 *
 * - ウィンドウを2枚作成し、それぞれに初期画像を流し込みます。
 * - カメラを2台（例）作成し、ウィンドウとの対応付けを登録します。
 * - HighGUI の初回表示を確定させるため、最初に present と waitKey 相当を1度だけ呼びます。
 * - マウスコールバックを全ウィンドウに設定します。
 * - 既定のキーバインドをインストールします。
 */
App::App()
{
  // --- 1枚目 ---
  windows_.emplace_back("Preview", win::Size{800, 600}, win::Point{100, 100});
  win::Window& first_window = windows_.back();
  first_window.create();
  first_window.setMonitorIndex(1);
  focused_id_ = first_window.id();  // フォーカスをこのウィンドウに設定

  // 1枚目の初期画像
  cv::Mat initial_image_preview = cv::Mat(
      first_window.size().height, first_window.size().width,
      CV_8UC3, cv::Scalar(30, 30, 30));
  cv::putText(initial_image_preview,
              "Hello HighGUI Preview",
              {40, 300},
              cv::FONT_HERSHEY_SIMPLEX,
              2.0,
              {200, 200, 255},
              3);
  first_window.setImage(std::move(initial_image_preview));

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

  // 2枚目の初期画像
  cv::Mat initial_image_second = cv::Mat(
      second_window.size().height, second_window.size().width,
      CV_8UC3, cv::Scalar(30, 30, 30));
  cv::putText(initial_image_second,
              "Hello HighGUI Second",
              {40, 300},
              cv::FONT_HERSHEY_SIMPLEX,
              2.0,
              {200, 200, 255},
              3);
  second_window.setImage(std::move(initial_image_second));

  // 2台目カメラ（例：index=4 に仮でぶら下げる）
  cameras_.emplace_back(/*index*/4, /*id*/2, "Cam1");
  if (!cameras_.back().open()) {
    LOG_ERROR("Camera open failed: index=4 (接続されていない可能性)");
  } else {
    cam_to_win_[2] = second_window.id();
  }

  // 初回表示を確定（HighGUIはwaitKey/pollKey経由で更新される）
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
      &App::onMouseCallback,  // キャプチャなしの関数ポインタ
      static_cast<void*>(context_ptr)
    );
  }

  // 既定のキーバインド
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
