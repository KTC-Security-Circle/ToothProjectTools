#include "serve/serve_app.hpp"

#include "logger/logger_macros.hpp"
#include "scan/scan_event.hpp"

#include <atomic>
#include <chrono>
#include <exception>
#include <thread>
#include <utility>

namespace serve {
namespace
{

control::ControlEvent toControlEvent(const scan::ScanEvent& scan_event)
{
  control::ControlFields fields;
  fields.reserve(scan_event.values.size());
  for (const auto& [key, value] : scan_event.values) {
    fields.emplace_back(key, value);
  }
  return control::ControlEvent{scan_event.event, std::move(fields)};
}

} // namespace

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

  std::atomic_bool running{true};
  std::jthread control_thread(
      [this, &running](std::stop_token stop_token) {
        runControlLoop(running, stop_token);
      });

  while (running.load()) {
    service_.windowService().processPendingRequests();
    if (service_.windowService().hasOpenWindows()) {
      service_.windowService().pollEvents(1);
    }
    for (const auto& event : service_.scanEventQueue().drain()) {
      writer_.writeEvent(toControlEvent(event));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  (void)service_.scanService().stopScan();
  for (int attempt = 0; attempt < 200; ++attempt) {
    service_.windowService().processPendingRequests();
    for (const auto& event : service_.scanEventQueue().drain()) {
      writer_.writeEvent(toControlEvent(event));
    }
    const auto status = service_.scanService().scanStatus();
    if (!status.ok || status.status == scan::ScanState::idle ||
        status.status == scan::ScanState::completed || status.status == scan::ScanState::failed ||
        status.status == scan::ScanState::stopped) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  service_.scanService().shutdown();
  for (const auto& event : service_.scanEventQueue().drain()) {
    writer_.writeEvent(toControlEvent(event));
  }

  service_.windowService().processPendingRequests();
  service_.projectorService().closeAll();
  service_.windowService().closeAllOnMainThread();

  control_thread.request_stop();
  if (control_thread.joinable()) {
    control_thread.join();
  }

  service_.shutdown();
  for (const auto& event : service_.scanEventQueue().drain()) {
    writer_.writeEvent(toControlEvent(event));
  }
  mjpeg_server_.stop();
  return 0;
}

void ServeApp::runControlLoop(std::atomic_bool& running, std::stop_token stop_token) {
  while (running.load() && !stop_token.stop_requested()) {
    auto read_result = reader_.read();
    if (read_result.status == control::ReadStatus::end_of_input) {
      running.store(false);
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
      const auto result = adapter_.handle(read_result.message);
      if (result == control::AdapterResult::shutdown) {
        running.store(false);
        break;
      }
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
}

} // namespace serve
