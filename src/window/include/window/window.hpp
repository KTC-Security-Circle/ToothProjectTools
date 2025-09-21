#pragma once
#include "window_types.hpp"
#include <opencv2/highgui.hpp>
#include <atomic>
#include <optional>

namespace win {

class Window {
public:
  // 論理的なウィンドウID（アプリ内一意）
  using Id = std::uint64_t;

  // 生成（OpenCV側のウィンドウ生成は create() で行う）
  explicit Window(std::string name,
                  Size size = {},
                  Point pos = {},
                  int monitor_index = 0,
                  LayoutMode layout = LayoutMode::Free,
                  bool visible = true,
                  bool fullscreen = false,
                  int z_index = 0,
                  int refresh_hz = 60);

  Id          id() const noexcept { return id_; }
  const std::string& name() const noexcept { return name_; }

  // 実ウィンドウ生成／破棄
  void create(int flags = cv::WINDOW_NORMAL);        // namedWindow
  void destroy();                                    // destroyWindow

  // 表示（imshow）。呼ぶと timestamp を更新
  void present(const cv::Mat& frame);

  // プロパティ操作
  void setVisible(bool on);
  void setFullscreen(bool on);
  void move(Point p);          // moveWindow
  void resize(Size s);         // resizeWindow
  void setLayout(LayoutMode m) { layout_ = m; }
  void setZIndex(int z) { z_index_ = z; } // HighGUIでは実質ダミー
  void setRefreshRate(int hz) { refresh_rate_hz_ = hz; }

  // 状態取得
  bool visible() const { return visible_; }
  bool fullscreen() const { return fullscreen_; }
  Size size() const { return size_; }
  Point pos() const { return pos_; }
  int monitorIndex() const { return monitor_index_; }
  LayoutMode layout() const { return layout_; }
  int zIndex() const { return z_index_; }
  int refreshRate() const { return refresh_rate_hz_; }

  // 最終表示の時刻
  std::chrono::steady_clock::time_point lastPresented() const { return last_presented_; }

  // イベント処理（HighGUIは waitKey が唯一のイベント取得口）
  static int pollEvents(int delay_ms = 1); // cv::waitKey
private:
  static Id nextId();

  Id id_;
  std::string name_;
  Size  size_;
  Point pos_;
  int   monitor_index_;
  LayoutMode layout_;
  bool  visible_;
  bool  fullscreen_;
  int   z_index_;
  int   refresh_rate_hz_;
  std::chrono::steady_clock::time_point last_presented_{};
  bool created_{false};
};

} // namespace win
