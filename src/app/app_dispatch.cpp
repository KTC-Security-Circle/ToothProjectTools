// =============================================================================
//  app_dispatch.cpp
//  コマンド処理（ルーティングと適用）を担当するモジュール
// =============================================================================

#include "app/app.hpp"
#include "logger/logger_macros.hpp"
#include "app/app_locals.hpp"

#include <type_traits>
#include <variant>
#include <vector>
#include <functional>
#include <algorithm> // ← find_if 用
#include <opencv2/imgcodecs.hpp> // cv::imwrite に必要
#include <filesystem>            // ディレクトリ作成用 (C++17)
#include <iomanip>               // 時刻フォーマット用
#include <sstream>               // 文字列構築用

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
/** @brief アプリ全体に直接作用するコマンドの処理 */
bool App::handleAppLevelCommand_(const Command& command) {
  bool is_handled = false;

  // App のメンバ関数内ラムダ（this 捕捉）なので private に合法アクセス可
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

    } else if constexpr (std::is_same_v<T, CmdCapturePush>) {
      const auto& cap = concrete_command;
      using CamId = Camera::Id;

      // 1. カメラIDの特定
      const CamId cam_id = cap.camera_id
                           ? static_cast<CamId>(*cap.camera_id)
                           : static_cast<CamId>(target_window.cameraId());

      if (static_cast<long>(cam_id) < 0) {
        LOG_WARN("保存スキップ: Camera ID未割り当て (win={})", target_window.id());
        return;
      }

      // 2. カメラの特定とフレーム取得
      auto it = std::find_if(this->cameras_.begin(), this->cameras_.end(),
                             [cam_id](const auto& c) { return c.id() == cam_id; });

      if (it == this->cameras_.end()) {
        LOG_ERROR("保存エラー: カメラデバイスが見つかりません (id={})", static_cast<unsigned long>(cam_id));
        return;
      }

      Camera& camera = *it;

      // カメラが開いていなければ一時的に開く
      bool needed_open = !camera.isOpened();
      if (needed_open) {
        if (!camera.open()) {
            LOG_ERROR("保存エラー: カメラオープン失敗 (id={})", static_cast<unsigned long>(cam_id));
            return;
        }
      }

      // フレーム取得
      cv::Mat frame = camera.getFrame();

      // 一時的に開いた場合は閉じる（運用によりますが、ここでは開けっ放しにせず戻す場合）
      // if (needed_open) camera.close(); 

      if (frame.empty()) {
        LOG_WARN("保存エラー: 取得フレームが空でした (id={})", static_cast<unsigned long>(cam_id));
        return;
      }

      // 3. ファイル名の生成 (例: captures/cam1_20251129_123456.png)
      // 保存先ディレクトリ
      const std::string save_dir = "captures";
      try {
          if (!fs::exists(save_dir)) {
              fs::create_directories(save_dir);
          }
      } catch (const std::exception& e) {
          LOG_ERROR("ディレクトリ作成失敗: {}", e.what());
          return;
      }

      // タイムスタンプ生成 (ミリ秒対応版)
      auto now = std::chrono::system_clock::now();
      std::time_t now_time = std::chrono::system_clock::to_time_t(now);
      std::tm tm_now = *std::localtime(&now_time);

      // 現在時刻のミリ秒部分を計算 (0-999)
      auto duration = now.time_since_epoch();
      auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

      std::stringstream ss;
      ss << save_dir << "/cam" << static_cast<unsigned long>(cam_id) << "_"
         << std::put_time(&tm_now, "%Y%m%d_%H%M%S")
         << "_" << std::setfill('0') << std::setw(3) << millis // ここに _000 ～ _999 を追加
         << ".png";
      
      std::string filepath = ss.str();

      // 4. 画像の保存 (cv::imwrite)
      // 圧縮パラメータ（PNG圧縮レベル3など）を指定する場合
      // std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 3};
      bool success = cv::imwrite(filepath, frame);

      if (success) {
        LOG_INFO("画像保存完了: {}", filepath);
      } else {
        LOG_ERROR("画像保存失敗: {}", filepath);
      }
    }
    // それ以外は no-op
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
      for (auto& w : this->windows_) {
        // existsAndVisible が無い前提：visible() で代替
        if (w.visible()) {
          LOG_INFO("  適用対象: Window id={}, name='{}'", w.id(), w.name());
          this->applyCommandToWindow_(w, dispatch_command.cmd);
        }
      }

    } else if constexpr (std::is_same_v<Target, TargetFocused>) {
      LOG_INFO("宛先: フォーカス中のウィンドウ id={}", this->focused_id_);
      if (auto* fw = this->findWindowById(this->focused_id_)) {
        if (fw->visible()) {
          LOG_INFO("  適用対象: Window id={}, name='{}'", fw->id(), fw->name());
          this->applyCommandToWindow_(*fw, dispatch_command.cmd);
        } else {
          LOG_INFO("  フォーカス中のウィンドウは不可視または存在しません");
        }
      } else {
        LOG_INFO("  フォーカス中のウィンドウは見つかりませんでした");
      }

    } else if constexpr (std::is_same_v<Target, TargetById>) {
      LOG_INFO("宛先: 指定IDのウィンドウ id={}", target_variant.id);
      if (auto* tw = this->findWindowById(target_variant.id)) {
        if (tw->visible()) {
          LOG_INFO("  適用対象: Window id={}, name='{}'", tw->id(), tw->name());
          this->applyCommandToWindow_(*tw, dispatch_command.cmd);
        } else {
          LOG_INFO("  指定IDのウィンドウは不可視または存在しません");
        }
      } else {
        LOG_INFO("  指定IDのウィンドウは見つかりませんでした");
      }
    }
  }, dispatch_command.target);

  // 3) 共通の後処理
  finalizeDispatch_();
}
