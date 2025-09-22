// src/cmd/include/cmd/dispatch_cmd.hpp
#pragma once
#include "cmd/target.hpp"
#include "cmd/commands.hpp"

struct DispatchCmd {
  Target  target;
  Command cmd;
};
