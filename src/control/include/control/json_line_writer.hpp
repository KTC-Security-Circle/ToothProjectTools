#pragma once

#include "control/control_message.hpp"

#include <mutex>
#include <ostream>
#include <string>

namespace control {

class JsonLineWriter {
public:
  explicit JsonLineWriter(std::ostream& output);

  void writeReady(const std::string& version);
  void writeResponse(const ControlResponse& response);
  void writeEvent(const ControlEvent& event);

private:
  static std::string escape(const std::string& value);
  static std::string serializeValue(const ControlValue& value);
  static void appendFields(std::string& json, const ControlFields& fields);
  void writeLine(std::string json);

  std::ostream& output_;
  std::mutex mutex_;
};

} // namespace control
