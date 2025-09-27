// =============================================================================
//  app_dispatch.cpp
//  コマンド処理（ルーティングと適用）を担当するモジュール
// =============================================================================

#include "app/app.hpp"
#include "logger/logger_macros.hpp"

#include "app/app_locals.hpp"

#include <type_traits>

// -----------------------------------------------------------------------------
/** @brief アプリ全体に直接作用するコマンドの処理
 *
 * ここではアプリケーションに対する操作（例：フォーカス移動など）を処理します。
 * 対象ウィンドウが不要なコマンドはここで完結させます。
 */
bool App::handleAppLevelCommand_(const Command& command) {
  bool is_handled = false;
  std::visit([&](auto&& concrete_command){
    using ConcreteCommandType = std::decay_t<decltype(concrete_command)>;
    if constexpr (std::is_same_v<ConcreteCommandType, CmdFocusNext>) {
      LOG_INFO("コマンド: フォーカス移動（次）");
      doFocusNext();
      is_handled = true;
    }
  }, command);
  return is_handled;
}

// -----------------------------------------------------------------------------
/** @brief 1ウィンドウへのコマンド適用
 *
 * フルスクリーン切替、モニタ移動、終了など、対象ウィンドウが明確な
 * コマンドの実行本体です。
 */
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

// -----------------------------------------------------------------------------
/** @brief 宛先: 全ウィンドウ
 *
 * 全てのウィンドウに対して applyCommandToWindow_ を適用します。
 * 不可視・未存在のウィンドウはスキップします。
 */
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

// -----------------------------------------------------------------------------
/** @brief 宛先: フォーカス中
 *
 * 現在フォーカスしているウィンドウに対してコマンドを適用します。
 */
void App::dispatchToFocused_(const DispatchCmd& dispatch_command) {
  LOG_INFO("宛先: フォーカス中のウィンドウ id={}", focused_id_);
  if (auto* focused_window_ptr = findWindowById(focused_id_)) {
    if (existsAndVisible(*focused_window_ptr)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'",
               focused_window_ptr->id(), focused_window_ptr->name());
      applyCommandToWindow_(*focused_window_ptr, dispatch_command.cmd);
    } else {
      LOG_INFO("  フォーカス中のウィンドウは不可視または存在しません");
    }
  } else {
    LOG_INFO("  フォーカス中のウィンドウは見つかりませんでした");
  }
}

// -----------------------------------------------------------------------------
/** @brief 宛先: 指定ID
 *
 * 指定IDのウィンドウが存在し、かつ可視ならコマンドを適用します。
 */
void App::dispatchToId_(const DispatchCmd& dispatch_command, WindowId target_window_id) {
  LOG_INFO("宛先: 指定IDのウィンドウ id={}", target_window_id);
  if (auto* target_window_ptr = findWindowById(target_window_id)) {
    if (existsAndVisible(*target_window_ptr)) {
      LOG_INFO("  適用対象: Window id={}, name='{}'",
               target_window_ptr->id(), target_window_ptr->name());
      applyCommandToWindow_(*target_window_ptr, dispatch_command.cmd);
    } else {
      LOG_INFO("  指定IDのウィンドウは不可視または存在しません");
    }
  } else {
    LOG_INFO("  指定IDのウィンドウは見つかりませんでした");
  }
}

// -----------------------------------------------------------------------------
/** @brief コマンド適用後の共通後処理
 *
 * 現状では 1 フレームだけ描画をスキップして処理を安定させます。
 * HighGUI の描画は waitKey/pollKey に依存しているため、ループ側で
 * 適切に processInput() が回る前提です。
 */
void App::finalizeDispatch_() {
  skip_render_once_ = true;
}

// -----------------------------------------------------------------------------
/** @brief コマンドの実行（メインハブ）
 *
 * - まずアプリ全体のコマンドを処理し、処理済みなら終了します。
 * - 次に宛先種別（全体／フォーカス／ID）に応じて適用します。
 * - 最後に共通の後処理を行います。
 */
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
