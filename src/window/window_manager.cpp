#include "window/window_manager.hpp"
#include "logger/logger_macros.hpp" // ロガーがある前提
#include <algorithm>

namespace win {

WindowId WindowManager::createWindow(const std::string& name, cv::Size size, cv::Point pos) {
  WindowId new_id = next_id_++;

  // Propsの構築
  WindowProps props;
  props.id = new_id;
  props.name = name;
  props.width = size.width;
  props.height = size.height;
  props.x = pos.x;
  props.y = pos.y;

  // Windowインスタンス生成
  auto window = std::make_unique<Window>(props);
  
  // OpenCVウィンドウの実体作成
  // ※ Window::create() 内で cv::namedWindow が呼ばれる設計
  window->create();

  LOG_INFO("WindowManager: ウィンドウ作成 id={}, name='{}'", new_id, name);

  windows_.push_back(std::move(window));
  return new_id;
}

Window* WindowManager::get(WindowId id) const {
  auto it = std::find_if(windows_.begin(), windows_.end(),
                         [id](const auto& w) { return w->id() == id; });
  
  if (it != windows_.end()) {
    return it->get();
  }
  return nullptr;
}

Window* WindowManager::getByName(const std::string& name) const {
  auto it = std::find_if(windows_.begin(), windows_.end(),
                         [&name](const auto& w) { return w->name() == name; });
  
  if (it != windows_.end()) {
    return it->get();
  }
  return nullptr;
}

bool WindowManager::closeWindow(WindowId id) {
  auto it = std::find_if(windows_.begin(), windows_.end(),
                         [id](const auto& w) { return w->id() == id; });

  if (it == windows_.end()) {
    return false;
  }

  if (*it) {
    (*it)->destroy();
  }
  windows_.erase(it);
  LOG_INFO("WindowManager: ウィンドウ破棄 id={}", id);
  return true;
}

void WindowManager::forEach(std::function<void(Window&)> action) {
  for (auto& w : windows_) {
    if (w) action(*w);
  }
}

void WindowManager::pollEvents(int delay_ms) {
  Window::pollEvents(delay_ms);
}

} // namespace win
