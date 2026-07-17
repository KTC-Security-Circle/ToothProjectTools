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
                service::scan_dataset::ScanDatasetValidationConfig config;
                if (!scan_dataset_command.input_dir.empty())
                {
                    config.input_dir = std::filesystem::path{scan_dataset_command.input_dir};
                }
                if (!scan_dataset_command.left_dir.empty())
                {
                    config.left_dir = std::filesystem::path{scan_dataset_command.left_dir};
                }
                if (!scan_dataset_command.right_dir.empty())
                {
                    config.right_dir = std::filesystem::path{scan_dataset_command.right_dir};
                }
                if (!scan_dataset_command.metadata_file.empty())
                {
                    config.metadata_file = std::filesystem::path{scan_dataset_command.metadata_file};
                }
                config.allow_partial = scan_dataset_command.allow_partial;
                return command_result_mapper::scan_dataset::toCommandResult(ctx.validator.validate(config));
            }
            return common::notHandled();
        },
        command);
}

} // namespace handler::scan_dataset
