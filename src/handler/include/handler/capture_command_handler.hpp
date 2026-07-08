#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace runtime
{
struct CaptureHandlerContext;
}

namespace handler::capture
{

/// @brief Capture系commandをCaptureServiceへdispatchする。
///
/// Args:
///   ctx <runtime::CaptureHandlerContext&>: CaptureService参照を持つhandler context。
///   command <const cmd::Command&>: dispatch対象のcommand variant。
///
/// Return:
///   <common::CommandResult>: Capture系commandの処理有無、成功可否、error、response用values。
common::CommandResult handle(runtime::CaptureHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::capture
