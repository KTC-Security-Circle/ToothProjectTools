#pragma once

#include "common/command_result.hpp"
#include "service/projector_result.hpp"

namespace command_result_mapper::projector
{

/// @brief ProjectorResultをCommandResultへ変換する。
///
/// Args:
///   result <const service::projector::ProjectorResult&>: ProjectorService実行結果。
///   include_window_role <bool>: response valuesへwindow_roleを含めるか。
///   include_size <bool>: response valuesへwidth/heightを含めるか。
///   include_pattern_count <bool>: response valuesへpattern_countを含めるか。
///   include_pattern_index <bool>: response valuesへpattern_indexを含めるか。
///
/// Return:
///   <common::CommandResult>: 共通command実行結果。
common::CommandResult toCommandResult(const service::projector::ProjectorResult& result, bool include_window_role,
                                      bool include_size, bool include_pattern_count, bool include_pattern_index);

/// @brief list_monitors結果をCommandResultへ変換する。
common::CommandResult toMonitorListCommandResult(const service::projector::ProjectorResult& result);

} // namespace command_result_mapper::projector
