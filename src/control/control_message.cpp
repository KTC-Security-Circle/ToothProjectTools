#include "control/control_message.hpp"

#include <utility>

namespace control {

ControlResponse ControlResponse::success(
    std::optional<std::string> id,
    ControlFields fields) {
  ControlResponse response;
  response.id = std::move(id);
  response.ok = true;
  response.fields = std::move(fields);
  return response;
}

ControlResponse ControlResponse::failure(
    std::optional<std::string> id,
    std::string code,
    std::string message) {
  ControlResponse response;
  response.id = std::move(id);
  response.error = ControlError{std::move(code), std::move(message)};
  return response;
}

} // namespace control
