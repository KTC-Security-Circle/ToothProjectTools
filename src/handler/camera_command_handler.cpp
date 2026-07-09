#include "handler/camera_command_handler.hpp"

#include "command_result_mapper/camera_command_result_mapper.hpp"
#include "service/sidecar_service.hpp"

#include <type_traits>
#include <variant>

namespace handler::camera
{

common::CommandResult handle(runtime::CameraHandlerContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](const auto& camera_command) -> common::CommandResult
        {
            using CommandType = std::decay_t<decltype(camera_command)>;
            if constexpr (std::is_same_v<CommandType, cmd::CmdOpenCamera>)
            {
                return command_result_mapper::camera::toCommandResult(
                    ctx.sidecar_service.openCamera(camera_command.camera_id, camera_command.role), true);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdCloseCamera>)
            {
                return command_result_mapper::camera::toCommandResult(
                    ctx.sidecar_service.closeCamera(camera_command.role), false);
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::camera
