#pragma once

#include "common/command_result.hpp"
#include "decode/decode_result.hpp"

namespace command_result_mapper::decode
{

/// @brief DecodePatternsResultをCommandResultへ変換する。
///
/// Args:
///   result <const service::decode::DecodePatternsResult&>: decode実行結果。
///
/// Return:
///   <common::CommandResult>: 共通command実行結果。
common::CommandResult toCommandResult(const service::decode::DecodePatternsResult& result);

} // namespace command_result_mapper::decode
