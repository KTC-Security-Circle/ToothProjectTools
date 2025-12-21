#pragma once

// =============================================================================
// Forward declarations / lightweight includes
// =============================================================================
#include "window_types.hpp"        // Size, Point, LayoutMode（軽量）
#include <cstdint>                 // std::uint64_t
#include <chrono>                  // timestamps
#include <string>                  // std::string
#include <atomic>
#include <opencv2/core/mat.hpp>

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

  // 明示：コピー禁止・ムーブ可
  Window() = delete; // ← これで「空名前の既定個体」を防ぐ
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) noexcept;
  Window& operator=(Window&&) noexcept;

  explicit Window(const WindowProps& props);

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
  void present();

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
  void setImage(const cv::Mat& img);
  void setImage(cv::Mat&& img);
  void setCameraId(int id) noexcept { camera_id = id; }

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
  [[nodiscard]] int  cameraId() const noexcept { return camera_id; }
  [[nodiscard]] std::chrono::steady_clock::time_point
                           lastPresented() const noexcept { return last_presented_; }
  [[nodiscard]] Size getMonitorSize() const;

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
  int   monitor_index_{1};
  LayoutMode layout_{LayoutMode::Free};
  bool   visible_{true};
  int    camera_id{-1};   // ★ このウィンドウに紐づくカメラ（-1: 未割当）
  bool  fullscreen_{false};
  int   z_index_{0};
  int   refresh_rate_hz_{60};

  // === 二重バッファ ===
  cv::Mat front_;                 // present() が読む（表示用）
  cv::Mat back_;                  // setImage() が書く（アップロード用）
  std::atomic<bool> dirty_{false}; // back_ に新フレームがある合図

  std::chrono::steady_clock::time_point last_presented_{};
  bool created_{false};
};

} // namespace win
