#include "input/input_handler.hpp"

void InputHandler::bind(int keycode, Action action) {
  map_[keycode] = std::move(action);
}

void InputHandler::unbind(int keycode) {
  map_.erase(keycode);
}

bool InputHandler::bound(int keycode) const {
  return map_.find(keycode) != map_.end();
}

void InputHandler::clear() {
  map_.clear();
}

void InputHandler::handle(int keycode) const {
  if (keycode < 0) return; // 未入力（HighGUIは未入力で -1 等を返す）
  if (auto it = map_.find(keycode); it != map_.end()) {
    it->second(); // ここが「void() を実行」
  }
}
