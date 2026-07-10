#include "command_result_mapper/projector_command_result_mapper.hpp"

#include <map>
#include <string>
#include <utility>

namespace command_result_mapper::projector
{

common::CommandResult toCommandResult(const service::projector::ProjectorResult& result, bool include_window_role,
                                      bool include_size, bool include_pattern_count, bool include_pattern_index)
{
    if (!result.ok)
    {
        if (result.error)
        {
            return common::failure(result.error->code, result.error->message);
        }
        return common::failure("internal_error", "projector command failed without error detail");
    }

    auto values = std::map<std::string, std::string>{{"projector_role", result.projector_role}};
    if (include_window_role && !result.window_role.empty())
    {
        values.emplace("window_role", result.window_role);
    }
    if (include_size && result.width > 0)
    {
        values.emplace("width", std::to_string(result.width));
    }
    if (include_size && result.height > 0)
    {
        values.emplace("height", std::to_string(result.height));
    }
    if (include_pattern_count && result.pattern_count > 0)
    {
        values.emplace("pattern_count", std::to_string(result.pattern_count));
    }
    if (include_pattern_index && result.pattern_index >= 0)
    {
        values.emplace("pattern_index", std::to_string(result.pattern_index));
    }
    return common::success(std::move(values));
}

} // namespace command_result_mapper::projector
