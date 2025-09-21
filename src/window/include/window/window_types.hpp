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

} // namespace win
