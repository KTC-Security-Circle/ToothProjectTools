#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"
#include "runtime/handler_context.hpp"

namespace handler::camera
{

/// @brief camera resource commandを処理する。
///
/// Args:
///   ctx <runtime::CameraHandlerContext&>: camera操作に必要なruntime context。
///   command <const cmd::Command&>: 処理対象command。
///
/// Return:
///   <common::CommandResult>: command処理結果。
common::CommandResult handle(runtime::CameraHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::camera
