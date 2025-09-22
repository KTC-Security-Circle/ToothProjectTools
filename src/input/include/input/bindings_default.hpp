#pragma once
#include "input_handler.hpp"
#include "window/window.hpp"
#include <deque>
#include "input/dispatch_cmd.hpp"

void install_default_bindings(
  InputHandler& handler,
  std::deque<DispatchCmd>& cmd_que
);
