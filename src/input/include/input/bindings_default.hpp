#pragma once
#include "input/input_handler.hpp"
#include "window/window.hpp"
#include "window/window_types.hpp"
#include <deque>
#include "cmd/dispatch_cmd.hpp"

void install_default_bindings(
  InputHandler& handler,
  std::deque<DispatchCmd>& cmd_que,
  win::WindowId projector_id
);
