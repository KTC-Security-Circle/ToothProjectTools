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
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

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
  
  // --- Camera 1 (Left想定) ---
  video::CameraOptions opt1;
  opt1.device_index = 4; // 環境に合わせて設定
  id_cam1_ = cam_mgr_.createCamera(opt1, "CamLeft");
  
  if (id_cam1_ != video::kInvalidCameraId) {
    cam_to_win_[id_cam1_] = id_preview_; // Previewに表示
  }

  // --- Camera 2 (Right想定) ---
  video::CameraOptions opt2;
  opt2.device_index = 6; // 環境に合わせて設定
  id_cam2_ = cam_mgr_.createCamera(opt2, "CamRight");
  
  if (id_cam2_ != video::kInvalidCameraId) {
    cam_to_win_[id_cam2_] = id_second_; // Secondに表示
  }

  // スキャン用カメラとして登録
  scan_cam_id_left_  = id_cam1_;
  scan_cam_id_right_ = id_cam2_;

  calibrator_ = std::make_unique<calib::Calibrator>();

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
  install_default_bindings(input_, cmd_que_, id_projector_);

  input_.bind('k', [this](){
      // 左カメラのキャリブレーションを実行
      // フォルダは事前に "captures/calibrationCameraL" に画像が入っている前提
      if (scan_cam_id_left_ != video::kInvalidCameraId) {
          cmd_que_.push_back(DispatchCmd{
              TargetAll{}, // ターゲットウィンドウは関係ないのでAll
              CmdCalibrate{ 
                  scan_cam_id_left_, 
                  "captures/calibrationCameraL" 
              }
          });
      }
      
      if (scan_cam_id_right_ != video::kInvalidCameraId) {
          cmd_que_.push_back(DispatchCmd{
              TargetAll{}, // ターゲットウィンドウは関係ないのでAll
              CmdCalibrate{ 
                  scan_cam_id_right_, 
                  "captures/calibrationCameraR" 
              }
          });
      }
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

  // 2) 自動スキャンロジック
  if (sl_system_ && sl_system_->isScanning()) {
      
      // 時間経過チェック
      if (sl_system_->checkTimerAndReset(scan_interval_ms_)) {
          
          // --- A. 撮影 (Capture L/R) -----------------------------
          // ※ ヘルパーラムダ: 指定IDのカメラから画像を撮ってバッファに入れる
          auto capture_and_store = [&](video::CameraId cid, std::vector<cv::Mat>& buf, const char* label) {
              cv::Mat frame;
              if (cid != video::kInvalidCameraId) {
                  if (auto* cam = cam_mgr_.get(cid)) {
                      frame = cam->getFrame(); // 最新フレーム取得
                  }
              }
              
              if (!frame.empty()) {
                  buf.push_back(frame.clone());
              } else {
                  LOG_WARN("Scan: {} フレーム取得失敗 (Skip)", label);
                  buf.push_back(cv::Mat()); // 欠損してもインデックスを合わせるため空画像を追加
              }
          };

          // 左右それぞれ撮影
          capture_and_store(scan_cam_id_left_,  scanned_imgs_left_,  "Left");
          capture_and_store(scan_cam_id_right_, scanned_imgs_right_, "Right");

          LOG_INFO("Scan: パターン {} 撮影 (L:{}, R:{})", 
                   sl_system_->getCurrentIndex(), scanned_imgs_left_.size(), scanned_imgs_right_.size());

          // --- B. 次のパターンへ (Advance) -----------------------
          int old_idx = sl_system_->getCurrentIndex();
          sl_system_->nextPattern(/*loop=*/false);
          int new_idx = sl_system_->getCurrentIndex();

          if (new_idx > old_idx) {
               // --- C. 投影 (Project) ---
               if (auto* proj = win_mgr_.get(id_projector_)) {
                   proj->setImage(sl_system_->getCurrentPatternImage());
               }
          } else {
               // --- D. 完了 (Finish) ---
               sl_system_->stopScan();
               
               // プロジェクタOFF
               if (auto* proj = win_mgr_.get(id_projector_)) {
                   cv::Mat black(proj->size().height, proj->size().width, CV_8UC3, cv::Scalar(0,0,0));
                   proj->setImage(black);
               }

               LOG_INFO("=== スキャン完了 ===");

               saveScanResults_();
               
               // 将来的なデコード処理への接続点
               // sl_system_->decodeStereo(scanned_imgs_left_, scanned_imgs_right_);
          }
      }
  }

  // 3) コマンド適用
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

void App::saveScanResults_() {
  if (scanned_imgs_left_.empty() && scanned_imgs_right_.empty()) {
    LOG_WARN("Save: 保存する画像がありません");
    return;
  }

  LOG_INFO("=== 画像保存を開始します... ===");

  // 1. 保存先ディレクトリの作成 (captures/scan_YYYYMMDD_HHMMSS)
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm = *std::localtime(&t);

  std::stringstream ss;
  ss << "captures/scan_" << std::put_time(&tm, "%Y%m%d_%H%M%S");
  std::string dir_path = ss.str();

  try {
    if (!fs::exists(dir_path)) {
      fs::create_directories(dir_path);
    }
  } catch (const std::exception& e) {
    LOG_ERROR("Save: ディレクトリ作成失敗: {}", e.what());
    return;
  }

  // 2. 画像保存ループ
  // ヘルパー: vectorを回して保存
  auto save_images = [&](const std::vector<cv::Mat>& imgs, const std::string& prefix) {
    for (size_t i = 0; i < imgs.size(); ++i) {
      if (imgs[i].empty()) continue;

      // ファイル名: left_00.png, right_00.png ...
      std::stringstream fname;
      fname << dir_path << "/" << prefix << "_" 
            << std::setfill('0') << std::setw(2) << i << ".png";
      
      // PNG圧縮パラメータ (0-9, 大きいほど高圧縮・遅い。3推奨)
      // 高速化したい場合は保存フォーマットを ".bmp" や ".jpg" に変えるか、
      // 別のスレッドで保存処理を行う必要があります。
      if (cv::imwrite(fname.str(), imgs[i])) {
         // 成功時はログ過多になるので、全部終わってから出すか、デバッグレベルで
         // LOG_DEBUG("Saved: {}", fname.str());
      } else {
         LOG_ERROR("Save: 書き込み失敗 {}", fname.str());
      }
    }
    LOG_INFO("Save: {}画像 {}枚 保存完了", prefix, imgs.size());
  };

  if (!scanned_imgs_left_.empty())  save_images(scanned_imgs_left_, "left");
  if (!scanned_imgs_right_.empty()) save_images(scanned_imgs_right_, "right");

  LOG_INFO("=== 全画像の保存が完了しました: {} ===", dir_path);
}
