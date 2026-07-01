#pragma once

#include "control/control_message.hpp"

namespace service {
class SidecarService;
struct SidecarResult;
}

namespace control {

class JsonLineWriter;

enum class AdapterResult {
  continue_running,
  shutdown,
};

class ControlInputAdapter {
public:
  ControlInputAdapter(service::SidecarService& service, JsonLineWriter& writer);

  AdapterResult handle(const ControlMessage& message);

private:
  void writeServiceFailure(
      const std::string& id,
      const service::SidecarResult& result);

  service::SidecarService& service_;
  JsonLineWriter& writer_;
};

} // namespace control
