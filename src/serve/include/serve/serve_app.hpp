#pragma once

#include "control/control_input_adapter.hpp"
#include "control/json_line_reader.hpp"
#include "control/json_line_writer.hpp"
#include "serve/sidecar_service.hpp"
#include "stream/mjpeg_server.hpp"
#include "stream/stream_registry.hpp"

#include <istream>
#include <ostream>
#include <atomic>
#include <stop_token>
#include <string>

namespace serve {

struct ServeOptions {
  std::string mjpeg_host{"127.0.0.1"};
  int mjpeg_port{39010};
};

class ServeApp {
public:
  ServeApp(
      ServeOptions options,
      std::istream& control_input,
      std::ostream& control_output);

  int run();

private:
  /// @brief JSON Lines control入力をcontrol threadで処理する。
  ///
  /// Args:
  ///   running <std::atomic_bool&>: main loop継続状態。
  ///   stop_token <std::stop_token>: jthread停止通知。
  ///
  /// Return:
  ///   <void>: なし。
  void runControlLoop(std::atomic_bool& running, std::stop_token stop_token);

  ServeOptions options_;
  stream::StreamRegistry streams_;
  service::SidecarService service_;
  stream::MjpegServer mjpeg_server_;
  control::JsonLineReader reader_;
  control::JsonLineWriter writer_;
  control::ControlInputAdapter adapter_;
};

} // namespace serve
