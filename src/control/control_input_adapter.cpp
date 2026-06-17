#include "control/control_input_adapter.hpp"

#include "control/json_line_writer.hpp"
#include "service/sidecar_service.hpp"

#include <cstdint>
#include <string>

namespace control {
namespace {

ControlResponse missingField(const std::string& id, const std::string& field) {
  return ControlResponse::failure(
      id, "missing_field", "missing required field: " + field);
}

} // namespace

ControlInputAdapter::ControlInputAdapter(
    service::SidecarService& service,
    JsonLineWriter& writer)
    : service_(service), writer_(writer) {}

AdapterResult ControlInputAdapter::handle(const ControlMessage& message) {
  if (!message.id || message.id->empty()) {
    writer_.writeResponse(ControlResponse::failure(
        std::nullopt, "missing_field", "missing required field: id"));
    return AdapterResult::continue_running;
  }
  const auto& id = *message.id;

  if (!message.cmd || message.cmd->empty()) {
    writer_.writeResponse(missingField(id, "cmd"));
    return AdapterResult::continue_running;
  }

  if (*message.cmd == "ping") {
    writer_.writeResponse(ControlResponse::success(
        id, {{"result", std::string{"pong"}}}));
    return AdapterResult::continue_running;
  }

  if (*message.cmd == "open_camera") {
    if (!message.camera_id) {
      writer_.writeResponse(missingField(id, "camera_id"));
      return AdapterResult::continue_running;
    }
    if (!message.role || message.role->empty()) {
      writer_.writeResponse(missingField(id, "role"));
      return AdapterResult::continue_running;
    }

    const auto result = service_.openCamera(*message.camera_id, *message.role);
    if (!result.ok) {
      writeServiceFailure(id, result);
      return AdapterResult::continue_running;
    }
    writer_.writeResponse(ControlResponse::success(id));
    writer_.writeEvent(ControlEvent{
        "camera_opened",
        {{"camera_id", static_cast<std::int64_t>(*message.camera_id)},
         {"role", *message.role}}});
    return AdapterResult::continue_running;
  }

  if (*message.cmd == "close_camera") {
    if (!message.role || message.role->empty()) {
      writer_.writeResponse(missingField(id, "role"));
      return AdapterResult::continue_running;
    }
    const auto result = service_.closeCamera(*message.role);
    if (!result.ok) {
      writeServiceFailure(id, result);
    } else {
      writer_.writeResponse(ControlResponse::success(id));
    }
    return AdapterResult::continue_running;
  }

  if (*message.cmd == "start_stream") {
    if (!message.role || message.role->empty()) {
      writer_.writeResponse(missingField(id, "role"));
      return AdapterResult::continue_running;
    }
    const auto result = service_.startStream(*message.role);
    if (!result.ok) {
      writeServiceFailure(id, result);
      return AdapterResult::continue_running;
    }
    writer_.writeResponse(ControlResponse::success(
        id, {{"url", result.value}}));
    writer_.writeEvent(ControlEvent{
        "stream_started",
        {{"role", *message.role}, {"url", result.value}}});
    return AdapterResult::continue_running;
  }

  if (*message.cmd == "stop_stream") {
    if (!message.role || message.role->empty()) {
      writer_.writeResponse(missingField(id, "role"));
      return AdapterResult::continue_running;
    }
    const auto result = service_.stopStream(*message.role);
    if (!result.ok) {
      writeServiceFailure(id, result);
    } else {
      writer_.writeResponse(ControlResponse::success(id));
    }
    return AdapterResult::continue_running;
  }

  if (*message.cmd == "capture_frame") {
    if (!message.role || message.role->empty()) {
      writer_.writeResponse(missingField(id, "role"));
      return AdapterResult::continue_running;
    }
    if (!message.output || message.output->empty()) {
      writer_.writeResponse(missingField(id, "output"));
      return AdapterResult::continue_running;
    }
    const auto result = service_.captureFrame(*message.role, *message.output);
    if (!result.ok) {
      writeServiceFailure(id, result);
      return AdapterResult::continue_running;
    }
    writer_.writeResponse(ControlResponse::success(
        id, {{"path", result.value}}));
    writer_.writeEvent(ControlEvent{
        "frame_saved",
        {{"role", *message.role}, {"path", result.value}}});
    return AdapterResult::continue_running;
  }

  if (*message.cmd == "shutdown") {
    writer_.writeResponse(ControlResponse::success(id));
    return AdapterResult::shutdown;
  }

  // TODO: headless用のDispatchCmd/Handler contextが整った段階で、
  // このcommand mappingを既存dispatch::execute経由へ移行する。
  writer_.writeResponse(ControlResponse::failure(
      id, "invalid_command", "unknown command: " + *message.cmd));
  return AdapterResult::continue_running;
}

void ControlInputAdapter::writeServiceFailure(
    const std::string& id,
    const service::SidecarResult& result) {
  writer_.writeResponse(ControlResponse::failure(
      id, result.error_code, result.error_message));
}

} // namespace control
