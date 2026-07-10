#include "handler/projector_command_handler.hpp"

#include "command_result_mapper/projector_command_result_mapper.hpp"
#include "service/projector_service.hpp"

#include <type_traits>
#include <variant>

namespace handler::projector
{

common::CommandResult handle(runtime::ProjectorHandlerContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](const auto& projector_command) -> common::CommandResult
        {
            using CommandType = std::decay_t<decltype(projector_command)>;
            if constexpr (std::is_same_v<CommandType, cmd::CmdListMonitors>)
            {
                return command_result_mapper::projector::toMonitorListCommandResult(ctx.projector_service.listMonitors());
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdConfigureProjectorSurface>)
            {
                const auto placement = projector_command.placement == "custom"
                                           ? service::projector::ProjectorPlacement::custom
                                           : service::projector::ProjectorPlacement::center;
                return command_result_mapper::projector::toCommandResult(
                    ctx.projector_service.configureSurface(service::projector::ProjectorSurfaceRequest{
                        projector_command.projector_role,
                        projector_command.monitor_index,
                        projector_command.width,
                        projector_command.height,
                        projector_command.x,
                        projector_command.y,
                        placement,
                    }),
                    true, false, false, false);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdOpenProjector>)
            {
                return command_result_mapper::projector::toCommandResult(
                    ctx.projector_service.openProjector(service::projector::ProjectorOpenConfig{
                        projector_command.projector_role,
                        projector_command.window_role,
                        projector_command.width,
                        projector_command.height,
                    }),
                    true, true, false, false);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdCloseProjector>)
            {
                return command_result_mapper::projector::toCommandResult(
                    ctx.projector_service.closeProjector(projector_command.projector_role), false, false, false, false);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdGeneratePatterns>)
            {
                return command_result_mapper::projector::toCommandResult(
                    ctx.projector_service.generatePatterns(projector_command.projector_role), false, true, true, false);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdProjectorShowPattern>)
            {
                return command_result_mapper::projector::toCommandResult(
                    ctx.projector_service.showPattern(projector_command.projector_role, projector_command.index), false, false, false, true);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdProjectorNextPattern>)
            {
                return command_result_mapper::projector::toCommandResult(
                    ctx.projector_service.nextPattern(projector_command.projector_role), false, false, false, true);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdProjectorPrevPattern>)
            {
                return command_result_mapper::projector::toCommandResult(
                    ctx.projector_service.prevPattern(projector_command.projector_role), false, false, false, true);
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::projector
