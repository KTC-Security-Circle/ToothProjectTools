#include "handler/scan_command_handler.hpp"

#include "command_result_mapper/scan_command_result_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/scan_service.hpp"

#include <filesystem>
#include <type_traits>
#include <variant>

namespace handler::scan
{

common::CommandResult handle(runtime::ScanHandlerContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](const auto& scan_command) -> common::CommandResult
        {
            using CommandType = std::decay_t<decltype(scan_command)>;
            if constexpr (std::is_same_v<CommandType, cmd::CmdStartScan>)
            {
                return command_result_mapper::scan::toCommandResult(ctx.scan_service.startScan(service::scan::ScanStartConfig{
                    scan_command.scan_id,
                    scan_command.projector_role,
                    scan_command.left_role,
                    scan_command.right_role,
                    std::filesystem::path{scan_command.output_dir},
                    scan_command.settle_ms,
                }));
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdScanStatus>)
            {
                return command_result_mapper::scan::toCommandResult(ctx.scan_service.scanStatus(scan_command.scan_id));
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdStopScan>)
            {
                return command_result_mapper::scan::toCommandResult(ctx.scan_service.stopScan(scan_command.scan_id));
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::scan
