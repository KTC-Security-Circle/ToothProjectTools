#include "command_result_adapters.hpp"

#include <map>
#include <string>
#include <utility>

namespace headless::result_adapter
{

common::CommandResult window(const win::WindowResult& result, bool include_size)
{
    if (!result.ok)
    {
        if (result.error)
        {
            return common::failure(result.error->code, result.error->message);
        }
        return common::failure("internal_error", "window command failed without error detail");
    }

    auto values = std::map<std::string, std::string>{
        {"window_role", result.role},
        {"window_id", std::to_string(result.window_id)},
    };
    if (include_size)
    {
        values.emplace("width", std::to_string(result.width));
        values.emplace("height", std::to_string(result.height));
    }
    return common::success(std::move(values));
}

} // namespace headless::result_adapter
