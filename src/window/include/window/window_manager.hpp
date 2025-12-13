#pragma once
#include "window.hpp"
#include "window_types.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace win {

class WindowManager {
public:
  WindowManager() = default;
  ~WindowManager() = default;

  // コピー禁止（unique_ptr管理のため）
  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  /**
   * @brief ウィンドウを生成し、管理下に置く
   * @return 自動生成された一意な WindowId
   */
  WindowId createWindow(const std::string& name, cv::Size size, cv::Point pos);

  /**
   * @brief IDからウィンドウを取得する
   * @return 存在すればポインタ、なければ nullptr
   */
  Window* get(WindowId id) const;

  /**
   * @brief 名前からウィンドウを取得する（デバッグ用など）
   */
  Window* getByName(const std::string& name) const;

  /**
   * @brief 全ウィンドウに対して処理を行う（描画ループなどで使用）
   */
  void forEach(std::function<void(Window&)> action);

  /**
   * @brief 全ウィンドウの数を返す
   */
  size_t count() const { return windows_.size(); }

private:
  // 所有権を保持
  std::vector<std::unique_ptr<Window>> windows_;
  
  // ID発行用カウンタ (1から開始)
  WindowId next_id_{1};
};

} // namespace win
