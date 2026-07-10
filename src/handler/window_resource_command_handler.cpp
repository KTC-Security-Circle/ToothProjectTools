#include "handler/window_resource_command_handler.hpp"

#include "command_result_mapper/window_command_result_mapper.hpp"
#include "service/window_service.hpp"

#include <type_traits>
#include <variant>

namespace handler::window_resource
{

common::CommandResult handle(runtime::WindowResourceHandlerContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](const auto& window_command) -> common::CommandResult
        {
            using CommandType = std::decay_t<decltype(window_command)>;
            if constexpr (std::is_same_v<CommandType, cmd::CmdOpenWindow>)
            {
                return command_result_mapper::window::toCommandResult(
                    ctx.window_service.openWindow(service::window::WindowOpenConfig{
                        window_command.window_role,
                        window_command.title,
                        window_command.width,
                        window_command.height,
                        window_command.monitor_index,
                        window_command.fullscreen,
                    }),
                    true);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdCloseWindow>)
            {
                return command_result_mapper::window::toCommandResult(
                    ctx.window_service.closeWindow(window_command.window_role), false);
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::window_resource
