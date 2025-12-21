#include "input/input_handler.hpp"
#include "logger/logger_macros.hpp" // LOG_INFO, LOG_WARN, LOG_DEBUG, LOG_TRACE などを定義したヘッダを想定

namespace input {

/**
 * @brief キー入力と対応するアクションをバインドする。
 */
void InputHandler::bind(int keycode, Action action) {
  map_[keycode] = std::move(action);
  LOG_INFO("キーコード {} にアクションをバインドしました", keycode);
}

/**
 * @brief 指定したキーコードのバインドを解除する。
 */
void InputHandler::unbind(int keycode) {
  if (map_.erase(keycode)) {
    LOG_INFO("キーコード {} のバインドを解除しました", keycode);
  } else {
    LOG_WARN("存在しないキーコード {} のバインド解除が試みられました", keycode);
  }
}

/**
 * @brief 指定したキーコードがバインド済みか確認する。
 */
bool InputHandler::bound(int keycode) const {
  bool result = map_.find(keycode) != map_.end();
  LOG_DEBUG("キーコード {} のバインド確認: {}", keycode, result ? "あり" : "なし");
  return result;
}

/**
 * @brief 登録されている全てのキー入力バインドを削除する。
 */
void InputHandler::clear() {
  size_t count = map_.size();
  map_.clear();
  LOG_INFO("{} 件のキー入力バインドを全て削除しました", count);
}

/**
 * @brief 入力されたキーコードに応じてバインド済みアクションを実行する。
 */
void InputHandler::handle(int keycode) const {
  if (keycode < 0) {
    LOG_TRACE("無効なキーコード {} を受信（入力なしとして無視）", keycode);
    return;
  }
  if (auto it = map_.find(keycode); it != map_.end()) {
    LOG_INFO("キーコード {} に対応するアクションを実行します", keycode);
    it->second();
  } else {
    LOG_DEBUG("キーコード {} はバインドされていません（処理なし）", keycode);
  }
}

}
