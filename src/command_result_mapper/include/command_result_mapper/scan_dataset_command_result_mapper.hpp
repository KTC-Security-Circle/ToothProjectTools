#pragma once

#include "common/command_result.hpp"
#include "scan/scan_dataset_result.hpp"

namespace command_result_mapper::scan_dataset
{

/// @brief ScanDatasetValidationResultをCommandResultへ変換する。
///
/// Args:
///   result <const ::scan::dataset::ScanDatasetValidationResult&>: scan dataset validation結果。
///
/// Return:
///   <common::CommandResult>: 共通command実行結果。
common::CommandResult toCommandResult(
    const ::scan::dataset::ScanDatasetValidationResult& result);

} // namespace command_result_mapper::scan_dataset
