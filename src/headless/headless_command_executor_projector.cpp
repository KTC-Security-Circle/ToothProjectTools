#include "headless/headless_command_executor.hpp"

#include "command_result_mapper/projector_command_result_mapper.hpp"
#include "command_result_mapper/window_command_result_mapper.hpp"
#include "service/projector_service.hpp"
#include "service/window_service.hpp"

namespace headless
{
common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdOpenWindow& command)
{
    return command_result_mapper::window::toCommandResult(
        window_service_.openWindow({command.window_role, command.title, command.width, command.height,
                                    command.monitor_index, command.fullscreen}), true);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCloseWindow& command)
{
    return command_result_mapper::window::toCommandResult(window_service_.closeWindow(command.window_role), false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdListMonitors&)
{
    return command_result_mapper::projector::toMonitorListCommandResult(projector_service_.listMonitors());
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdConfigureProjectorSurface& command)
{
    const auto placement = command.placement == "custom" ? service::projector::ProjectorPlacement::custom
                                                          : service::projector::ProjectorPlacement::center;
    return command_result_mapper::projector::toCommandResult(
        projector_service_.configureSurface({command.projector_role, command.monitor_index, command.width,
                                             command.height, command.x, command.y, placement}),
        true, false, false, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdOpenProjector& command)
{
    return command_result_mapper::projector::toCommandResult(
        projector_service_.openProjector({command.projector_role, command.window_role,
                                          command.width, command.height}),
        true, true, false, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCloseProjector& command)
{
    return command_result_mapper::projector::toCommandResult(
        projector_service_.closeProjector(command.projector_role), false, false, false, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdGeneratePatterns& command)
{
    return command_result_mapper::projector::toCommandResult(
        projector_service_.generatePatterns(command.projector_role), false, true, true, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdProjectorShowPattern& command)
{
    return command_result_mapper::projector::toCommandResult(
        projector_service_.showPattern(command.projector_role, command.index), false, false, false, true);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdProjectorNextPattern& command)
{
    return command_result_mapper::projector::toCommandResult(
        projector_service_.nextPattern(command.projector_role), false, false, false, true);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdProjectorPrevPattern& command)
{
    return command_result_mapper::projector::toCommandResult(
        projector_service_.prevPattern(command.projector_role), false, false, false, true);
}
} // namespace headless
