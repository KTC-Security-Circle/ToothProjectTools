#pragma once
#include <variant>

struct CmdToggleFullscreen { };
struct CmdMoveToMonitor   { int index; };
struct CmdQuit            { };
struct CmdFocusNext {};

using Command = std::variant<
  CmdToggleFullscreen,
  CmdMoveToMonitor,
  CmdQuit,
  CmdFocusNext
>;
