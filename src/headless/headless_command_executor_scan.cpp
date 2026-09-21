#include "headless/headless_command_executor.hpp"

#include "command_result_adapters.hpp"
#include "decode/decode_service.hpp"
#include "scan/scan_dataset_validator.hpp"
#include "scan/scan_service.hpp"

#include <filesystem>

namespace headless
{
common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdStartScan& command)
{
    return result_adapter::scan(scan_service_.startScan({
        command.scan_id, command.projector_role, command.left_role, command.right_role,
        std::filesystem::path{command.output_dir}, command.photodiode_device, command.photodiode_baud,
        command.sync_timeout_ms, command.sync_guard_ms, command.max_patterns}));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdScanStatus& command)
{
    return result_adapter::scan(scan_service_.scanStatus(command.scan_id));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdStopScan& command)
{
    return result_adapter::scan(scan_service_.stopScan(command.scan_id));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdValidateScanDataset& command)
{
    scan::dataset::ScanDatasetValidationConfig config;
    if (!command.input_dir.empty()) config.input_dir = std::filesystem::path{command.input_dir};
    if (!command.left_dir.empty()) config.left_dir = std::filesystem::path{command.left_dir};
    if (!command.right_dir.empty()) config.right_dir = std::filesystem::path{command.right_dir};
    if (!command.metadata_file.empty()) config.metadata_file = std::filesystem::path{command.metadata_file};
    config.allow_partial = command.allow_partial;
    return result_adapter::scanDataset(scan_dataset_validator_.validate(config));
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdDecodePatterns& command)
{
    decode::DecodePatternsConfig config;
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
    return result_adapter::decode(decode_service_.decodePatterns(config));
}
} // namespace headless
