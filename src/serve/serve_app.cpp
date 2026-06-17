#include "serve/serve_app.hpp"

#include "logger/logger_macros.hpp"

#include <exception>
#include <utility>

namespace serve {

ServeApp::ServeApp(
    ServeOptions options,
    std::istream& control_input,
    std::ostream& control_output)
    : options_(std::move(options)),
      service_(options_.mjpeg_host, options_.mjpeg_port, streams_),
      mjpeg_server_(options_.mjpeg_host, options_.mjpeg_port, streams_),
      reader_(control_input),
      writer_(control_output),
      adapter_(service_, writer_) {}

int ServeApp::run() {
  if (!mjpeg_server_.start()) {
    LOG_ERROR("Failed to start MJPEG server: {}", mjpeg_server_.lastError());
    return 1;
  }

  writer_.writeReady("0.1.0");

  bool running = true;
  while (running) {
    auto read_result = reader_.read();
    if (read_result.status == control::ReadStatus::end_of_input) {
      break;
    }
    if (read_result.status == control::ReadStatus::invalid) {
      writer_.writeResponse(control::ControlResponse::failure(
          read_result.response_id,
          read_result.error_code,
          read_result.error_message));
      continue;
    }

    try {
      running = adapter_.handle(read_result.message) != control::AdapterResult::shutdown;
    } catch (const std::exception& error) {
      LOG_ERROR("Unhandled sidecar command error: {}", error.what());
      writer_.writeResponse(control::ControlResponse::failure(
          read_result.message.id,
          "internal_error",
          "internal command processing error"));
    } catch (...) {
      LOG_ERROR("Unhandled non-standard sidecar command error");
      writer_.writeResponse(control::ControlResponse::failure(
          read_result.message.id,
          "internal_error",
          "internal command processing error"));
    }
  }

  service_.shutdown();
  mjpeg_server_.stop();
  return 0;
}

} // namespace serve
