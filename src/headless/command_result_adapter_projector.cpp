#include "command_result_adapters.hpp"

#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace headless::result_adapter
{
namespace
{

std::string boolString(bool value)
{
    return value ? "true" : "false";
}

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (const auto ch : value)
    {
        if (ch == '\\' || ch == '"')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

std::string monitorsJson(const std::vector<win::MonitorInfo>& monitors)
{
    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < monitors.size(); ++index)
    {
        const auto& monitor = monitors[index];
        if (index > 0)
        {
            stream << ',';
        }
        stream << "{\"monitor_index\":" << monitor.monitor_index << ",\"x\":" << monitor.x << ",\"y\":" << monitor.y
               << ",\"width\":" << monitor.width << ",\"height\":" << monitor.height
               << ",\"primary\":" << (monitor.primary ? "true" : "false") << ",\"name\":\"" << jsonEscape(monitor.name)
               << "\",\"fallback\":" << (monitor.fallback ? "true" : "false") << "}";
    }
    stream << ']';
    return stream.str();
}

} // namespace

common::CommandResult projector(const ::projector::ProjectorResult& result, bool include_window_role,
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
    if (result.code_width > 0)
    {
        values.emplace("code_width", std::to_string(result.code_width));
    }
    if (result.code_height > 0)
    {
        values.emplace("code_height", std::to_string(result.code_height));
    }
    if (include_pattern_count && result.pattern_count > 0)
    {
        values.emplace("pattern_count", std::to_string(result.pattern_count));
    }
    if (include_pattern_index && result.pattern_index >= 0)
    {
        values.emplace("pattern_index", std::to_string(result.pattern_index));
    }
    if (result.surface_width > 0)
    {
        values.emplace("monitor_index", std::to_string(result.monitor_index));
        values.emplace("monitor_x", std::to_string(result.monitor_x));
        values.emplace("monitor_y", std::to_string(result.monitor_y));
        values.emplace("monitor_width", std::to_string(result.monitor_width));
        values.emplace("monitor_height", std::to_string(result.monitor_height));
        values.emplace("surface_width", std::to_string(result.surface_width));
        values.emplace("surface_height", std::to_string(result.surface_height));
        values.emplace("pattern_width", std::to_string(result.pattern_width));
        values.emplace("pattern_height", std::to_string(result.pattern_height));
        values.emplace("pattern_x", std::to_string(result.pattern_x));
        values.emplace("display_width", std::to_string(result.display_width));
        values.emplace("display_height", std::to_string(result.display_height));
        values.emplace("display_x", std::to_string(result.display_x));
        values.emplace("display_y", std::to_string(result.display_y));
        values.emplace("pattern_y", std::to_string(result.pattern_y));
        values.emplace("clamped", boolString(result.clamped));
        values.emplace("marker_x", std::to_string(result.marker_x));
        values.emplace("marker_y", std::to_string(result.marker_y));
        values.emplace("marker_width", std::to_string(result.marker_width));
        values.emplace("marker_height", std::to_string(result.marker_height));
        values.emplace("photodiode_marker_mode", result.photodiode_marker_mode);
    }
    return common::success(std::move(values));
}

common::CommandResult monitorList(const ::projector::ProjectorResult& result)
{
    if (!result.ok)
    {
        if (result.error)
        {
            return common::failure(result.error->code, result.error->message);
        }
        return common::failure("internal_error", "monitor list command failed without error detail");
    }

    return common::success({
        {"monitor_count", std::to_string(result.monitors.size())},
        {"monitors_json", monitorsJson(result.monitors)},
    });
}

} // namespace headless::result_adapter
