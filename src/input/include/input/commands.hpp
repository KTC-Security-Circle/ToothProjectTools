#pragma once
#include <variant>

struct CmdToggleFullscreen { };
struct CmdMoveToMonitor   { int index; };
struct CmdQuit            { };

using Command = std::variant<
  CmdToggleFullscreen,
  CmdMoveToMonitor,
  CmdQuit
>;
