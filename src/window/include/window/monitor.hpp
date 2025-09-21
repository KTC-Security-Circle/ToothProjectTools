#pragma once
#include <vector>

namespace win {

struct MonitorRect {
  int x;
  int y;
  int width;
  int height;
};

// X11(Xrandr) でモニタ矩形を列挙（原点は仮想デスクトップ座標）
std::vector<MonitorRect> enumerate_monitors_x11();

} // namespace win
