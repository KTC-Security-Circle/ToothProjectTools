#include "input/input_handler.hpp"
#include "logger/logger_macros.hpp" // LOG_INFO, LOG_WARN, LOG_DEBUG などを定義したヘッダを想定

namespace input {

namespace
{
constexpr bool is_ascii_key(int raw_key) noexcept
{
    return raw_key >= 0 && raw_key <= 0x7F;
}
} // namespace

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

  const int raw_key = keycode;
  LOG_INFO("Key Input: Raw={} (0x{:X})", raw_key, raw_key);

  // HighGUI / X11 の特殊キーは ASCII でないので、ここで無視する。
  if (!is_ascii_key(raw_key)) {
    LOG_DEBUG("Special Key Ignored: Raw={} (0x{:X})", raw_key, raw_key);
    return;
  }

  if (auto it = map_.find(raw_key); it != map_.end()) {
    it->second();
  } else {
    LOG_WARN("Unbound Key: {} (Raw=0x{:X})", raw_key, raw_key);
  }
}

}
