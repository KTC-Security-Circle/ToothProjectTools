#include "command_result_adapters.hpp"

#include <map>
#include <string>
#include <utility>

namespace headless::result_adapter
{

common::CommandResult scan(const ::scan::ScanResult& result)
{
    if (!result.ok)
    {
        if (result.error)
        {
            return common::failure(result.error->code, result.error->message);
        }
        return common::failure("scan_failed", "scan command failed without error detail");
    }

    std::map<std::string, std::string> values{
        {"scan_id", result.scan_id},
        {"status", ::scan::toString(result.status)},
        {"projector_role", result.projector_role},
        {"left_role", result.left_role},
        {"right_role", result.right_role},
        {"output_dir", result.output_dir},
        {"pattern_count", std::to_string(result.pattern_count)},
        {"captured_count", std::to_string(result.captured_count)},
        {"current_index", std::to_string(result.current_index)},
    };
    return common::success(std::move(values));
}

} // namespace headless::result_adapter
