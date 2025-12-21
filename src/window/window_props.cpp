// =============================================================================
//  window_props.cpp
//  ウィンドウのプロパティ操作（可視状態、フルスクリーン、移動・リサイズ）
// =============================================================================

#include "window/window.hpp"
#include "window/monitor.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>

// -----------------------------------------------------------------------------
namespace win {

// 可視状態
void Window::setVisible(bool is_on) noexcept {
  visible_ = is_on;
  LOG_DEBUG("可視状態を設定: name='{}', 可視={}", name_, visible_);
}

// フルスクリーン
void Window::setFullscreen(bool is_on) {
  fullscreen_ = is_on;
  cv::setWindowProperty(
      name_, cv::WND_PROP_FULLSCREEN,
      is_on ? cv::WINDOW_FULLSCREEN : cv::WINDOW_NORMAL);
  LOG_INFO("フルスクリーンを設定: name='{}', 全画面={}", name_, fullscreen_);
}

// 移動
void Window::move(Point new_position) {
  pos_ = new_position; // ローカル座標を更新
  
  if (created_) {
    // 1. 指定されたモニタ番号の矩形を取得
    //    rect.x が「前のモニタの幅を足したオフセット」になっている
    auto rect_opt = get_monitor_rect(monitor_index_);
    
    int global_x = 0;
    int global_y = 0;

    if (rect_opt) {
        // モニタが見つかった場合: モニタ原点 + ローカル座標
        global_x = rect_opt->x + pos_.x;
        global_y = rect_opt->y + pos_.y;
        
        // ログ: どのモニタのどの位置に置いたか確認
        LOG_DEBUG("Window Move: Monitor{}({}, {}) + Local({}, {}) -> Global({}, {})", 
                  monitor_index_, rect_opt->x, rect_opt->y, pos_.x, pos_.y, global_x, global_y);
    } else {
        // 取得失敗時は安全策としてローカル座標をそのまま使う（メインモニタに出る）
        global_x = pos_.x;
        global_y = pos_.y;
    }

    cv::moveWindow(name_, global_x, global_y);
  }
}

// リサイズ
void Window::resize(Size new_size) {
  size_ = new_size;
  if (created_) {
    cv::resizeWindow(name_, size_.width, size_.height);
    LOG_DEBUG("ウィンドウサイズ変更: name='{}' -> {}x{}",
              name_, size_.width, size_.height);
  }
}

} // namespace win
