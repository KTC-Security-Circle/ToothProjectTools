// src/cmd/include/cmd/dispatch_cmd.hpp
#pragma once
#include "cmd/target.hpp"
#include "cmd/commands.hpp"

struct DispatchCmd {
  cmd::Target  target;
  cmd::Command cmd;
};
