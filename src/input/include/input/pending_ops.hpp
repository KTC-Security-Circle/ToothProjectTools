#pragma once

struct PendingOps {
  bool toggle_fullscreen = false;
  bool move_to_monitor_1 = false;
  bool move_to_monitor_2 = false;
  bool quit = false;                  // ★ 終了を表すフラグを追加
};
