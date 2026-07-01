#include "control/json_line_writer.hpp"

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace control {

JsonLineWriter::JsonLineWriter(std::ostream& output) : output_(output) {}

void JsonLineWriter::writeReady(const std::string& version) {
  writeEvent(ControlEvent{"ready", {{"version", version}}});
}

void JsonLineWriter::writeResponse(const ControlResponse& response) {
  std::string json{"{"};
  bool has_field = false;
  if (response.id) {
    json += "\"id\":\"" + escape(*response.id) + "\"";
    has_field = true;
  }
  if (has_field) {
    json += ',';
  }
  json += std::string{"\"ok\":"} + (response.ok ? "true" : "false");

  appendFields(json, response.fields);
  if (response.error) {
    json += ",\"error\":{\"code\":\"" + escape(response.error->code) +
            "\",\"message\":\"" + escape(response.error->message) + "\"}";
  }
  json += '}';
  writeLine(std::move(json));
}

void JsonLineWriter::writeEvent(const ControlEvent& event) {
  std::string json = "{\"event\":\"" + escape(event.event) + "\"";
  appendFields(json, event.fields);
  json += '}';
  writeLine(std::move(json));
}

std::string JsonLineWriter::escape(const std::string& value) {
  std::ostringstream escaped;
  for (unsigned char character : value) {
    switch (character) {
    case '\"':
      escaped << "\\\"";
      break;
    case '\\':
      escaped << "\\\\";
      break;
    case '\b':
      escaped << "\\b";
      break;
    case '\f':
      escaped << "\\f";
      break;
    case '\n':
      escaped << "\\n";
      break;
    case '\r':
      escaped << "\\r";
      break;
    case '\t':
      escaped << "\\t";
      break;
    default:
      if (character < 0x20) {
        escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<int>(character) << std::dec;
      } else {
        escaped << static_cast<char>(character);
      }
      break;
    }
  }
  return escaped.str();
}

std::string JsonLineWriter::serializeValue(const ControlValue& value) {
  return std::visit(
      [](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return "\"" + escape(item) + "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
          return item ? "true" : "false";
        } else {
          return std::to_string(item);
        }
      },
      value);
}

void JsonLineWriter::appendFields(std::string& json, const ControlFields& fields) {
  for (const auto& [name, value] : fields) {
    json += ",\"" + escape(name) + "\":" + serializeValue(value);
  }
}

void JsonLineWriter::writeLine(std::string json) {
  std::lock_guard lock(mutex_);
  output_ << json << '\n' << std::flush;
}

} // namespace control
