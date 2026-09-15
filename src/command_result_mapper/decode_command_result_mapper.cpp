#include "command_result_mapper/decode_command_result_mapper.hpp"

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace command_result_mapper::decode
{
namespace
{

std::string doubleString(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    return stream.str();
}

} // namespace

common::CommandResult toCommandResult(const ::decode::DecodePatternsResult& result)
{
    if (!result.ok)
    {
        if (result.error)
        {
            return common::failure(result.error->code, result.error->message);
        }
        return common::failure("decode_failed", "decode command failed without error detail");
    }

    return common::success({
        {"input_dir", result.input_dir},
        {"output_dir", result.output_dir},
        {"scan_id", result.scan_id},
        {"pattern_count", std::to_string(result.pattern_count)},
        {"image_width", std::to_string(result.image_width)},
        {"image_height", std::to_string(result.image_height)},
        {"projector_width", std::to_string(result.projector_width)},
        {"projector_height", std::to_string(result.projector_height)},
        {"threshold", std::to_string(result.threshold)},
        {"left_valid_count", std::to_string(result.left_valid_count)},
        {"right_valid_count", std::to_string(result.right_valid_count)},
        {"left_valid_ratio", doubleString(result.left_valid_ratio)},
        {"right_valid_ratio", doubleString(result.right_valid_ratio)},
    });
}

} // namespace command_result_mapper::decode
