#pragma once
#include <cstdint>
#include <chrono>
#include <string>

namespace win {

struct Size {
  int width{640};
  int height{480};
};

struct Point {
  int x{100};
  int y{100};
};

enum class LayoutMode {
  Free,       // HighGUI標準の自由配置
  Centered,   // 中央寄せ（自前計算で moveWindow）
  Tiled       // タイル配置（将来の拡張用）
};

using WindowId = std::uint32_t;
constexpr WindowId kInvalidWindowId = 0;

// ウィンドウ作成・管理に必要なプロパティ
struct WindowProps {
  WindowId    id{kInvalidWindowId};
  std::string name;
  
  // 座標・サイズ
  int         width{640};
  int         height{480};
  int         x{0};
  int         y{0};
  
  // モニタ・表示設定
  int         monitor_index{0}; // 0=未指定, 1~=モニタ番号
  LayoutMode  layout{LayoutMode::Free};
  bool        visible{true};
  bool        fullscreen{false};
  
  // その他
  int         z_index{0};
  int         refresh_hz{60};
};

} // namespace win
