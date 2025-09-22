#pragma once
#include "input_handler.hpp"
#include "window/window.hpp"
#include <deque>
#include "input/commands.hpp"

void install_default_bindings(
  InputHandler& handler,
  std::deque<Command>& cmd_que
);
