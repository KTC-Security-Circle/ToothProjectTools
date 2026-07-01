#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace control {

using ControlValue = std::variant<std::string, std::int64_t, bool>;
using ControlFields = std::vector<std::pair<std::string, ControlValue>>;

struct ControlMessage {
  std::optional<std::string> id;
  std::optional<std::string> cmd;
  std::optional<std::string> role;
  std::optional<std::string> output;
  std::optional<int> camera_id;
};

struct ControlError {
  std::string code;
  std::string message;
};

struct ControlResponse {
  std::optional<std::string> id;
  bool ok{false};
  ControlFields fields;
  std::optional<ControlError> error;

  static ControlResponse success(
      std::optional<std::string> id,
      ControlFields fields = {});
  static ControlResponse failure(
      std::optional<std::string> id,
      std::string code,
      std::string message);
};

struct ControlEvent {
  std::string event;
  ControlFields fields;
};

} // namespace control
