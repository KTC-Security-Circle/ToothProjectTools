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
        switch (ch)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

std::string issuesJson(const std::vector<scan::dataset::ScanDatasetIssue>& issues)
{
    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < issues.size(); ++index)
    {
        const auto& issue = issues[index];
        if (index > 0)
        {
            stream << ',';
        }
        stream << "{\"code\":\"" << jsonEscape(issue.code)
               << "\",\"message\":\"" << jsonEscape(issue.message)
               << "\",\"path\":\"" << jsonEscape(issue.path)
               << "\",\"pattern_index\":" << issue.pattern_index << '}';
    }
    stream << ']';
    return stream.str();
}

} // namespace

common::CommandResult scanDataset(const ::scan::dataset::ScanDatasetValidationResult& result)
{
    if (!result.ok)
    {
        return common::failure("internal_error", "scan dataset validation failed");
    }

    return common::success({
        {"input_dir", result.input_dir},
        {"scan_id", result.scan_id},
        {"valid", boolString(result.valid)},
        {"partial", boolString(result.partial)},
        {"pattern_count", std::to_string(result.pattern_count)},
        {"left_count", std::to_string(result.left_count)},
        {"right_count", std::to_string(result.right_count)},
        {"missing_count", std::to_string(result.missing_count)},
        {"issue_count", std::to_string(result.issues.size())},
        {"width", std::to_string(result.width)},
        {"height", std::to_string(result.height)},
        {"issues_json", issuesJson(result.issues)},
    });
}

} // namespace headless::result_adapter
