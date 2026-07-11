#include "handler/scan_dataset_command_handler.hpp"

#include "command_result_mapper/scan_dataset_command_result_mapper.hpp"
#include "runtime/handler_context.hpp"
#include "service/scan_dataset_validator.hpp"

#include <filesystem>
#include <type_traits>
#include <variant>

namespace handler::scan_dataset
{

common::CommandResult handle(runtime::ScanDatasetHandlerContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](const auto& scan_dataset_command) -> common::CommandResult
        {
            using CommandType = std::decay_t<decltype(scan_dataset_command)>;
            if constexpr (std::is_same_v<CommandType, cmd::CmdValidateScanDataset>)
            {
                return command_result_mapper::scan_dataset::toCommandResult(
                    ctx.validator.validate(service::scan_dataset::ScanDatasetValidationConfig{
                        std::filesystem::path{scan_dataset_command.input_dir},
                        scan_dataset_command.allow_partial,
                    }));
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::scan_dataset
