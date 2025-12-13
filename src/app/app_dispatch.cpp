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
      // パターン投影
      if (this->sl_system_) {
          int idx = concrete_command.index;
          // インデックスの範囲チェック
          if (idx >= 0 && idx < (int)this->sl_system_->getPatternCount()) {
              
              // 1. 画像をセット
              target_window.setImage(this->sl_system_->getPattern(idx));
              
              // 2. ログ出力
              LOG_INFO("パターン投影: index={} -> Window id={}", idx, target_window.id());
              
              // 3. 現在のインデックス状態を更新する
              this->current_pattern_index_ = idx; 
          }
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
