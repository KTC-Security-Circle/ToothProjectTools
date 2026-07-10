#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"
#include "runtime/handler_context.hpp"

namespace handler::projector
{

/// @brief projector commandを処理する。
///
/// Args:
///   ctx <runtime::ProjectorHandlerContext&>: ProjectorServiceを持つruntime context。
///   command <const cmd::Command&>: 処理対象command。
///
/// Return:
///   <common::CommandResult>: command処理結果。
common::CommandResult handle(runtime::ProjectorHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::projector
