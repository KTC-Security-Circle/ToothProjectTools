// =============================================================================
//  app_dispatch.cpp
//  コマンド処理（ルーティングと適用）を担当するモジュール
// =============================================================================

#include "app/app.hpp"
#include "logger/logger_macros.hpp"
#include "app/app_locals.hpp"
#include "cmd/commands.hpp" // CmdShowPatternのために必要

#include <type_traits>
#include <variant>
#include <vector>
#include <functional>
#include <algorithm> 
#include <opencv2/imgcodecs.hpp> 
#include <filesystem>            
#include <iomanip>               
#include <sstream>               

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
/** @brief アプリ全体に直接作用するコマンドの処理 */
bool App::handleAppLevelCommand_(const Command& command) {
  bool is_handled = false;

  std::visit([&](auto&& concrete_command){
    using T = std::decay_t<decltype(concrete_command)>;
    if constexpr (std::is_same_v<T, CmdFocusNext>) {
      LOG_INFO("コマンド: フォーカス移動（次）");
      this->doFocusNext();
      is_handled = true;
    }
    // それ以外は no-op
  }, command);

  return is_handled;
}

// -----------------------------------------------------------------------------
/** @brief 単一ウィンドウへのコマンド適用 */
void App::applyCommandToWindow_(win::Window& target_window, const Command& command) {
  std::visit([&](auto&& concrete_command){
    using T = std::decay_t<decltype(concrete_command)>;

    if constexpr (std::is_same_v<T, CmdToggleFullscreen>) {
      LOG_INFO("コマンド: フルスクリーン切替 -> Window id={}, name='{}'",
               target_window.id(), target_window.name());
      target_window.setFullscreen(!target_window.fullscreen());

    } else if constexpr (std::is_same_v<T, CmdMoveToMonitor>) {
      LOG_INFO("コマンド: モニタ移動 index={} -> Window id={}, name='{}'",
               concrete_command.index, target_window.id(), target_window.name());
      target_window.setMonitorIndex(concrete_command.index);

    } else if constexpr (std::is_same_v<T, CmdQuit>) {
      LOG_INFO("コマンド: 終了要求 -> アプリ全体に適用");
      this->running_ = false;

    } else if constexpr (std::is_same_v<T, CmdShowPattern>) {
      if (this->sl_system_) {
          // 指定インデックスへ移動（内部で時刻リセットされる）
          this->sl_system_->setIndex(concrete_command.index);
          
          // 表示更新
          target_window.setImage(this->sl_system_->getCurrentPatternImage());
          LOG_INFO("パターン指定: index={}", this->sl_system_->getCurrentIndex());
      }

    } else if constexpr (std::is_same_v<T, CmdNextPattern>) {
      if (this->sl_system_) {
          this->sl_system_->nextPattern(/*loop=*/true);
          
          target_window.setImage(this->sl_system_->getCurrentPatternImage());
          LOG_INFO("パターン(次): index={}", this->sl_system_->getCurrentIndex());
      }

    } else if constexpr (std::is_same_v<T, CmdPrevPattern>) {
      if (this->sl_system_) {
          this->sl_system_->prevPattern(/*loop=*/true);
          
          target_window.setImage(this->sl_system_->getCurrentPatternImage());
          LOG_INFO("パターン(前): index={}", this->sl_system_->getCurrentIndex());
      }

    } else if constexpr (std::is_same_v<T, CmdStartScan>) {
        if (!this->sl_system_) {
            LOG_ERROR("スキャン開始失敗: StructuredLight未初期化");
            return;
        }
        
        // 設定保存
        this->scan_interval_ms_ = concrete_command.interval_ms;
        this->scanned_imgs_left_.clear();
        this->scanned_imgs_right_.clear();
        
        // StructuredLight側の状態をリセット＆開始
        this->sl_system_->startScan(); 
        
        // 最初のパターン(index 0)を投影
        target_window.setImage(this->sl_system_->getCurrentPatternImage());
        
        LOG_INFO("=== 自動スキャン開始 (間隔: {}ms, 枚数: {}) ===", 
                 this->scan_interval_ms_, this->sl_system_->getPatternCount());

    // スキャン中断
    } else if constexpr (std::is_same_v<T, CmdStopScan>) {
        if (this->sl_system_ && this->sl_system_->isScanning()) {
            this->sl_system_->stopScan();
            LOG_INFO("=== 自動スキャン中断 ===");
        }

    } else if constexpr (std::is_same_v<T, CmdCalibrate>) {
        
        if (!this->calibrator_) {
            LOG_ERROR("Calib: Calibratorが初期化されていません");
            return;
        }

        const auto& target_id = concrete_command.target_camera_id;
        const auto& folder = concrete_command.image_folder;

        LOG_INFO("Calib: 開始 -> CameraID={}, Folder='{}'", target_id, folder);

        // 1. フォルダから画像ファイルをリストアップ
        std::vector<std::string> image_files;
        try {
            if (!fs::exists(folder)) {
                LOG_ERROR("Calib: フォルダが見つかりません {}", folder);
                return;
            }
            for (const auto& entry : fs::directory_iterator(folder)) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    // 拡張子フィルタ（簡易的）
                    if (ext == ".png" || ext == ".bmp" || ext == ".jpg") {
                        image_files.push_back(entry.path().string());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Calib: ファイル探索エラー {}", e.what());
            return;
        }

        if (image_files.empty()) {
            LOG_WARN("Calib: 指定フォルダに画像がありません");
            return;
        }

        // 2. 計算実行 (重い処理なので本来は別スレッド推奨だが、今回はコマンド内で実行)
        cv::Mat cam_mat, dist_coeffs;
        double rms = this->calibrator_->runCalibration(image_files, cam_mat, dist_coeffs);

        if (rms > 0 && rms < 1.0) { // RMSが1.0未満なら良好とされることが多い
            LOG_INFO("Calib: 成功! RMS Error = {}", rms);
            
            // 3. カメラにセット
            if (auto* camera = this->cam_mgr_.get(target_id)) {
                camera->setIntrinsics(cam_mat);
                camera->setDistCoeffs(dist_coeffs);
                LOG_INFO("Calib: パラメータをカメラ({})に適用しました", camera->name());
                
                // 確認のためコンソール出力
                // std::cout << "Camera Matrix:\n" << cam_mat << std::endl;
                // std::cout << "Dist Coeffs:\n" << dist_coeffs << std::endl;
            } else {
                LOG_ERROR("Calib: 対象カメラインスタンスが見つかりません");
            }

        } else if (rms >= 1.0) {
            LOG_WARN("Calib: 精度不良 (RMS={})。パラメータは適用されません。撮影環境を見直してください。", rms);
        } else {
            LOG_ERROR("Calib: 失敗 (チェッカーボードが検出できませんでした)");
        }

    } else if constexpr (std::is_same_v<T, CmdCapturePush>) {
      const auto& cap = concrete_command;

      using CamId = video::CameraId;

      // 1. カメラIDの特定
      // ※ Window側でcameraId()を持たせていない場合、cam_to_win_ から逆引きが必要ですが、
      // ここでは簡略化のため cap.camera_id が指定されている前提か、あるいはマップから探します。
      
      CamId cam_id = video::kInvalidCameraId;
      if (cap.camera_id) {
          cam_id = static_cast<CamId>(*cap.camera_id);
      } else {
          // target_window に紐づくカメラを探す
          // Appクラスで cam_to_win_ を管理しているので、そこから逆引き検索（重いですが）
          for (const auto& [cid, wid] : this->cam_to_win_) {
              if (wid == target_window.id()) {
                  cam_id = cid;
                  break;
              }
          }
      }

      if (cam_id == video::kInvalidCameraId) {
        LOG_WARN("保存スキップ: 対象ウィンドウ(id={})に紐づくカメラが見つかりません", target_window.id());
        return;
      }

      // 2. カメラの特定とフレーム取得 (Manager経由)
      // ★修正: this->cameras_ -> cam_mgr_.get(id)
      video::Camera* camera_ptr = this->cam_mgr_.get(cam_id);

      if (!camera_ptr) {
        LOG_ERROR("保存エラー: カメラデバイスが見つかりません (id={})", cam_id);
        return;
      }

      video::Camera& camera = *camera_ptr; // ★修正: video::Camera

      // カメラが開いていなければ一時的に開く
      bool needed_open = !camera.isOpened();
      if (needed_open) {
        if (!camera.open()) {
            LOG_ERROR("保存エラー: カメラオープン失敗 (id={})", cam_id);
            return;
        }
      }

      // フレーム取得
      cv::Mat frame = camera.getFrame();

      // 一時的に開いた場合は閉じる（運用による）
      // if (needed_open) camera.close(); 

      if (frame.empty()) {
        LOG_WARN("保存エラー: 取得フレームが空でした (id={})", cam_id);
        return;
      }

      // 3. ファイル名の生成
      const std::string save_dir = "captures";
      try {
          if (!fs::exists(save_dir)) {
              fs::create_directories(save_dir);
          }
      } catch (const std::exception& e) {
          LOG_ERROR("ディレクトリ作成失敗: {}", e.what());
          return;
      }

      // タイムスタンプ生成
      auto now = std::chrono::system_clock::now();
      std::time_t now_time = std::chrono::system_clock::to_time_t(now);
      std::tm tm_now = *std::localtime(&now_time);
      auto duration = now.time_since_epoch();
      auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

      std::stringstream ss;
      ss << save_dir << "/cam" << cam_id << "_"
         << std::put_time(&tm_now, "%Y%m%d_%H%M%S")
         << "_" << std::setfill('0') << std::setw(3) << millis
         << ".png";
      
      std::string filepath = ss.str();

      // 4. 画像の保存
      bool success = cv::imwrite(filepath, frame);
      if (success) {
        LOG_INFO("画像保存完了: {}", filepath);
      } else {
        LOG_ERROR("画像保存失敗: {}", filepath);
      }
    }
  }, command);
}

// -----------------------------------------------------------------------------
/** @brief コマンド適用後の共通後処理 */
void App::finalizeDispatch_() {
  skip_render_once_ = true;
}

// -----------------------------------------------------------------------------
/** @brief コマンドの実行（メインハブ） */
void App::dispatch(const DispatchCmd& dispatch_command) {
  // 1) 先にアプリ全体に直接作用するものを処理
  if (handleAppLevelCommand_(dispatch_command.cmd)) {
    finalizeDispatch_();
    return;
  }

  // 2) 宛先に応じてルーティング
  std::visit([&](auto&& target_variant){
    using Target = std::decay_t<decltype(target_variant)>;

    if constexpr (std::is_same_v<Target, TargetAll>) {
      LOG_INFO("宛先: 全ウィンドウ");
      // ★修正: windows_ ループ -> win_mgr_.forEach
      this->win_mgr_.forEach([this, &dispatch_command](win::Window& w){
          if (w.visible()) {
              this->applyCommandToWindow_(w, dispatch_command.cmd);
          }
      });

    } else if constexpr (std::is_same_v<Target, TargetFocused>) {
      LOG_INFO("宛先: フォーカス中のウィンドウ id={}", this->focused_id_);
      // ★修正: findWindowById -> win_mgr_.get
      if (auto* fw = this->win_mgr_.get(this->focused_id_)) {
        if (fw->visible()) {
          this->applyCommandToWindow_(*fw, dispatch_command.cmd);
        }
      }

    } else if constexpr (std::is_same_v<Target, TargetById>) { // ★win::TargetById -> TargetById
      LOG_INFO("宛先: 指定ID id={}", target_variant.id);
      // ★修正: findWindowById -> win_mgr_.get
      if (auto* tw = this->win_mgr_.get(target_variant.id)) {
        if (tw->visible()) {
          this->applyCommandToWindow_(*tw, dispatch_command.cmd);
        }
      }
    }
  }, dispatch_command.target);

  // 3) 共通の後処理
  finalizeDispatch_();
}
