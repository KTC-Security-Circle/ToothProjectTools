#pragma once
#include <vector>
#include <optional>

namespace win {

struct MonitorRect {
  int x;
  int y;
  int width;
  int height;
};

// --- Low Level (OS依存) ---
// X11(Xrandr) でモニタ矩形を列挙（順序は接続順などで、左から順とは限らない）
std::vector<MonitorRect> enumerate_monitors_x11();

// --- High Level (アプリロジック用) ---
// モニタインデックス (1-based: 1, 2, 3...) を指定して矩形を取得する
// 内部で X座標順（左→右）にソートを行うため、直感的な番号でアクセス可能
std::optional<MonitorRect> get_monitor_rect(int monitor_index);

} // namespace win
