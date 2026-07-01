#include "headless/headless_command_mapper.hpp"

#include "control/control_message.hpp"
#include "service/sidecar_service.hpp"

#include <filesystem>
#include <string>
#include <utility>

namespace headless
{
namespace
{

/// @brief HeadlessCommandError付きのCommandMapResult失敗値を作成する。
///
/// Args:
///   code <std::string>: sidecar responseへ返すerror code。
///   message <std::string>: sidecar responseへ返すerror message。
///
/// Return:
///   <CommandMapResult>: command変換失敗を表す結果。
CommandMapResult mapFailure(std::string code, std::string message)
{
    CommandMapResult result;
    result.error = HeadlessCommandError{std::move(code), std::move(message)};
    return result;
}

} // namespace

HeadlessCommandMapper::HeadlessCommandMapper(service::SidecarService& sidecar_service)
    : sidecar_service_(sidecar_service)
{
}

CommandMapResult HeadlessCommandMapper::mapCaptureFrame(const control::ControlMessage& message)
{
    if (!message.role || message.role->empty())
    {
        return mapFailure("missing_field", "missing required field: role");
    }

    if (!message.output || message.output->empty())
    {
        return mapFailure("missing_field", "missing required field: output");
    }

    const auto camera_id = sidecar_service_.resolveCameraId(*message.role);
    if (!camera_id)
    {
        return mapFailure("camera_not_open", "camera role is not open: " + *message.role);
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCaptureFrame{
        *camera_id,
        std::filesystem::path{*message.output},
    };
    return result;
}

} // namespace headless
