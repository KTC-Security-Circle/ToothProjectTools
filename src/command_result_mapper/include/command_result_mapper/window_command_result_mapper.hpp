#pragma once

#include "common/command_result.hpp"
#include "service/window_result.hpp"

namespace command_result_mapper::window
{

/// @brief WindowResultをCommandResultへ変換する。
///
/// Args:
///   result <const service::window::WindowResult&>: WindowService実行結果。
///   include_size <bool>: response valuesへwidth/heightを含めるか。
///
/// Return:
///   <common::CommandResult>: 共通command実行結果。
common::CommandResult toCommandResult(const service::window::WindowResult& result, bool include_size);

} // namespace command_result_mapper::window
