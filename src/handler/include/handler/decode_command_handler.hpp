#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace runtime
{
struct DecodeHandlerContext;
}

namespace handler::decode
{

/// @brief decode commandを処理する。
///
/// Args:
///   ctx <runtime::DecodeHandlerContext&>: DecodeServiceを持つruntime context。
///   command <const cmd::Command&>: 処理対象command。
///
/// Return:
///   <common::CommandResult>: command処理結果。
common::CommandResult handle(runtime::DecodeHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::decode
