#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace runtime
{
struct ScanDatasetHandlerContext;
}

namespace handler::scan_dataset
{

/// @brief scan dataset commandを処理する。
///
/// Args:
///   ctx <runtime::ScanDatasetHandlerContext&>: ScanDatasetValidatorを持つruntime context。
///   command <const cmd::Command&>: 処理対象command。
///
/// Return:
///   <common::CommandResult>: command処理結果。
common::CommandResult handle(
    runtime::ScanDatasetHandlerContext& ctx,
    const cmd::Command& command);

} // namespace handler::scan_dataset
