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

void InputHandler::handle(int keycode) const {
  if (keycode < 0) return;

  // 下位8ビットと、生の値を両方出力して確認する
  int clean_key = keycode & 0xFF;
  
  // ★デバッグ用: このログで実際の値を確認してください
  LOG_INFO("Key Input: Raw={} (0x{:X}), Clean={} (0x{:X}) -> Char='{}'", 
           keycode, keycode, clean_key, clean_key, 
           (clean_key >= 32 && clean_key <= 126) ? static_cast<char>(clean_key) : '?');

  if (auto it = map_.find(clean_key); it != map_.end()) {
    it->second();
  } else {
    // 未登録キーのログ（確認用）
    LOG_WARN("Unbound Key: {}", clean_key);
  }
}

}
