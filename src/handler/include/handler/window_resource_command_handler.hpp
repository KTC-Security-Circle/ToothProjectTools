#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"
#include "runtime/handler_context.hpp"

namespace handler::window_resource
{

/// @brief window resource commandを処理する。
///
/// Args:
///   ctx <runtime::WindowResourceHandlerContext&>: WindowServiceを持つruntime context。
///   command <const cmd::Command&>: 処理対象command。
///
/// Return:
///   <common::CommandResult>: command処理結果。
common::CommandResult handle(runtime::WindowResourceHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::window_resource
