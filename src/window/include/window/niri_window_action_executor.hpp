#pragma once
#include "window/window_service.hpp"
#include <optional>
#include <string>
#include <vector>
namespace win {
std::optional<std::vector<std::string>> niriActionArguments(const std::string& action);
class NiriWindowActionExecutor final : public WindowActionExecutor {
 public:
  WindowActionResult execute(win::WindowId, const std::optional<std::string>&,
                             const std::optional<std::string>&) override;
};
}
