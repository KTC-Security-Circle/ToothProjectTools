#pragma once

#include "control/control_message.hpp"

#include <istream>
#include <optional>
#include <string>

namespace control {

enum class ReadStatus {
  message,
  invalid,
  end_of_input,
};

struct ReadResult {
  ReadStatus status{ReadStatus::end_of_input};
  ControlMessage message;
  std::optional<std::string> response_id;
  std::string error_code;
  std::string error_message;
};

class JsonLineReader {
public:
  explicit JsonLineReader(std::istream& input);

  ReadResult read();

private:
  std::istream& input_;
};

} // namespace control
