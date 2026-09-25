#include "headless/headless_command_executor.hpp"

#include "command_result_adapters.hpp"
#include "projector/projector_service.hpp"
#include "window/window_service.hpp"

namespace headless
{
common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdOpenWindow& command)
{
    return result_adapter::window(
        window_service_.openWindow({command.window_role, command.title, command.width, command.height,
                                    command.monitor_index, command.fullscreen}), true);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCloseWindow& command)
{
    return result_adapter::window(window_service_.closeWindow(command.window_role), false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdListMonitors&)
{
    return result_adapter::monitorList(projector_service_.listMonitors());
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdConfigureProjectorSurface& command)
{
    const auto placement = command.placement == "custom" ? projector::ProjectorPlacement::custom
                                                          : projector::ProjectorPlacement::center;
    return result_adapter::projector(
        projector_service_.configureSurface({command.projector_role, command.monitor_index, command.width,
                                             command.height, command.x, command.y, placement}),
        true, false, false, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdOpenProjector& command)
{
    return result_adapter::projector(
        projector_service_.openProjector({command.projector_role, command.window_role,
                                          command.width, command.height}),
        true, true, false, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCloseProjector& command)
{
    return result_adapter::projector(
        projector_service_.closeProjector(command.projector_role), false, false, false, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdGeneratePatterns& command)
{
    return result_adapter::projector(
        projector_service_.generatePatterns(command.projector_role), false, true, true, false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdProjectorShowPattern& command)
{
    std::optional<projector::PhotodiodeMarkerMode> marker_mode;
    if (command.photodiode_marker_mode)
        marker_mode = *command.photodiode_marker_mode == "locate"
                          ? projector::PhotodiodeMarkerMode::locate
                          : projector::PhotodiodeMarkerMode::sync;
    return result_adapter::projector(
        projector_service_.showPattern(command.projector_role, command.index, marker_mode), false, false, false, true);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdProjectorNextPattern& command)
{
    return result_adapter::projector(
        projector_service_.nextPattern(command.projector_role), false, false, false, true);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdProjectorPrevPattern& command)
{
    return result_adapter::projector(
        projector_service_.prevPattern(command.projector_role), false, false, false, true);
}
} // namespace headless
