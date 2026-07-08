#include "control/control_response_adapter.hpp"

#include <map>

namespace control
{
namespace
{

/// @brief CommandResult valuesをControlFieldsへ変換する。
///
/// Args:
///   values <const std::map<std::string, std::string>&>: command成功時に返す追加値。
///
/// Return:
///   <ControlFields>: ControlResponseへ設定するfield配列。
ControlFields toControlFields(const std::map<std::string, std::string>& values)
{
    ControlFields fields;
    fields.reserve(values.size());
    for (const auto& [key, value] : values)
    {
        fields.emplace_back(key, value);
    }
    return fields;
}

} // namespace

ControlResponse toControlResponse(
    const std::string& id,
    const common::CommandResult& result)
{
    if (!result.handled)
    {
        return ControlResponse::failure(id, "invalid_command", "unsupported command");
    }

    if (!result.ok)
    {
        const auto code = result.error ? result.error->code : std::string{"command_failed"};
        const auto message = result.error ? result.error->message : std::string{"command failed without error detail"};
        return ControlResponse::failure(id, code, message);
    }

    return ControlResponse::success(id, toControlFields(result.values));
}

} // namespace control
