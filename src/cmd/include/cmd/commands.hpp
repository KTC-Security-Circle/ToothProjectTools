// src/cmd/include/cmd/commands.hpp
#pragma once
#include <variant>
#include <optional>
#include <string>

struct CmdToggleFullscreen { };
struct CmdMoveToMonitor   { int index; };
struct CmdQuit            { };
struct CmdFocusNext       { };

// プッシュ撮影のスコープ種別
enum class CaptureScope {
  FocusedOnly,  // フォーカス中の window_id のみ撮影
  CameraGroup   // 同じ camera_id に紐づく全 window_id を撮影
};

// プッシュ撮影コマンド
struct CmdCapturePush {
  CaptureScope           scope{CaptureScope::FocusedOnly};
  std::optional<int>     camera_id{};  // 省略時はフォーカス中ウィンドウの camera_id を使用
  std::string            tag{};        // 任意ラベル（保存名等に使う想定）
};

struct CmdShowPattern { 
    int index; 
  };

using Command = std::variant<
  CmdToggleFullscreen,
  CmdMoveToMonitor,
  CmdQuit,
  CmdFocusNext,
  CmdCapturePush,
  CmdShowPattern
>;
