#include "headless/headless_command_executor.hpp"

#include "command_result_mapper/decode_command_result_mapper.hpp"
#include "command_result_mapper/scan_command_result_mapper.hpp"
#include "command_result_mapper/scan_dataset_command_result_mapper.hpp"
#include "decode/decode_service.hpp"
#include "scan/scan_dataset_validator.hpp"
#include "scan/scan_service.hpp"

#include <filesystem>

namespace headless
{
common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdStartScan& command)
{
    return command_result_mapper::scan::toCommandResult(scan_service_.startScan({
        command.scan_id, command.projector_role, command.left_role, command.right_role,
        std::filesystem::path{command.output_dir}, command.settle_ms, command.sync_source,
        command.sync_timeout_ms, command.sync_guard_ms, command.sync_stable_frames,
        command.roi_x, command.roi_y, command.roi_width, command.roi_height,
        command.roi_black_threshold, command.roi_white_threshold}));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdScanStatus& command)
{
    return command_result_mapper::scan::toCommandResult(scan_service_.scanStatus(command.scan_id));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdStopScan& command)
{
    return command_result_mapper::scan::toCommandResult(scan_service_.stopScan(command.scan_id));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdValidateScanDataset& command)
{
    service::scan_dataset::ScanDatasetValidationConfig config;
    if (!command.input_dir.empty()) config.input_dir = std::filesystem::path{command.input_dir};
    if (!command.left_dir.empty()) config.left_dir = std::filesystem::path{command.left_dir};
    if (!command.right_dir.empty()) config.right_dir = std::filesystem::path{command.right_dir};
    if (!command.metadata_file.empty()) config.metadata_file = std::filesystem::path{command.metadata_file};
    config.allow_partial = command.allow_partial;
    return command_result_mapper::scan_dataset::toCommandResult(scan_dataset_validator_.validate(config));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdDecodePatterns& command)
{
    service::decode::DecodePatternsConfig config;
    if (!command.input_dir.empty()) config.input_dir = std::filesystem::path{command.input_dir};
    if (!command.left_dir.empty()) config.left_dir = std::filesystem::path{command.left_dir};
    if (!command.right_dir.empty()) config.right_dir = std::filesystem::path{command.right_dir};
    if (!command.metadata_file.empty()) config.metadata_file = std::filesystem::path{command.metadata_file};
    config.output_dir = std::filesystem::path{command.output_dir};
    config.threshold = command.threshold;
    config.allow_partial = command.allow_partial;
    config.projector_width = command.projector_width;
    config.projector_height = command.projector_height;
    config.pattern_count = command.pattern_count;
    return command_result_mapper::decode::toCommandResult(decode_service_.decodePatterns(config));
}
} // namespace headless
