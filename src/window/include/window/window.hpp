#pragma once

// =============================================================================
// Forward declarations / lightweight includes
// =============================================================================
#include "window_types.hpp"        // Size, Point, LayoutMode（軽量）
#include <cstdint>                 // std::uint64_t
#include <chrono>                  // timestamps
#include <string>                  // std::string

// OpenCV は .cpp に寄せる：ここでは cv::Mat のみ前方宣言にする
// → ヘッダ依存の縮小（再コンパイル抑制）
namespace cv { class Mat; }        // OK: メンバ/引数で参照のみなら前方宣言可
// 参照先の完全型が必要なのは実装側（.cpp）だけ。:contentReference[oaicite:1]{index=1}

namespace win {

// =============================================================================
// Window: HighGUI ウィンドウの薄いラッパ
// - 生成は create()、破棄は destroy()
// - present() で描画し、pollEvents() でイベント進行（waitKey）
// =============================================================================
class Window {
public:
  // ---------------------------------------------------------------------------
  // 型・生成
  // ---------------------------------------------------------------------------
  using Id = std::uint64_t;  // 論理ID（アプリ内一意）

  explicit Window(std::string name,
                  Size size = {},
                  Point pos = {},
                  int monitor_index = 0,
                  LayoutMode layout = LayoutMode::Free,
                  bool visible = true,
                  bool fullscreen = false,
                  int z_index = 0,
                  int refresh_hz = 60);

  // ---------------------------------------------------------------------------
  // 識別子・メタ
  // ---------------------------------------------------------------------------
  [[nodiscard]] Id id() const noexcept { return id_; }
  [[nodiscard]] const std::string& name() const noexcept { return name_; }

  // ---------------------------------------------------------------------------
  // ライフサイクル
  // ---------------------------------------------------------------------------
  void create(int flags = 0 /* cv::WINDOW_NORMAL を .cpp 側で使用 */);
  void destroy() noexcept;

  // ---------------------------------------------------------------------------
  // 描画
  // ---------------------------------------------------------------------------
  void present(const cv::Mat& frame); // 描画時に timestamp 更新

  // ---------------------------------------------------------------------------
  // 変更（プロパティ・操作）
  // ---------------------------------------------------------------------------
  void setVisible(bool on) noexcept;
  void setFullscreen(bool on);
  void move(Point p);
  void resize(Size s);
  void setLayout(LayoutMode m) noexcept { layout_ = m; }
  void setZIndex(int z)       noexcept { z_index_ = z; } // HighGUIでは実質ダミー
  void setRefreshRate(int hz) noexcept { refresh_rate_hz_ = hz; }
  // モニタ切り替え（指定 index が無効なら 1 にフォールバック）
  void setMonitorIndex(int new_index);


  // ---------------------------------------------------------------------------
  // 取得（状態）
  // ---------------------------------------------------------------------------
  [[nodiscard]] bool        visible()     const noexcept { return visible_; }
  [[nodiscard]] bool        fullscreen()  const noexcept { return fullscreen_; }
  [[nodiscard]] Size        size()        const noexcept { return size_; }
  [[nodiscard]] Point       pos()         const noexcept { return pos_; }
  [[nodiscard]] int         monitorIndex()const noexcept { return monitor_index_; }
  [[nodiscard]] LayoutMode  layout()      const noexcept { return layout_; }
  [[nodiscard]] int         zIndex()      const noexcept { return z_index_; }
  [[nodiscard]] int         refreshRate() const noexcept { return refresh_rate_hz_; }
  [[nodiscard]] std::chrono::steady_clock::time_point
                           lastPresented() const noexcept { return last_presented_; }

  // ---------------------------------------------------------------------------
  // イベント（HighGUI は waitKey のみ）
  // ---------------------------------------------------------------------------
  static int pollEvents(int delay_ms = 1); // cv::waitKey を .cpp で呼ぶ

private:
  static Id nextId();  // 実装は .cpp（std::atomic などは .cpp に閉じ込める）

  // ---------------------------------------------------------------------------
  // データ（公開インターフェースで必要な最小限）
  // ---------------------------------------------------------------------------
  Id id_{};
  std::string name_;

  Size  size_{};
  Point pos_{};
  int   monitor_index_{0};
  LayoutMode layout_{LayoutMode::Free};
  bool  visible_{true};
  bool  fullscreen_{false};
  int   z_index_{0};
  int   refresh_rate_hz_{60};

  std::chrono::steady_clock::time_point last_presented_{};
  bool created_{false};
};

} // namespace win
