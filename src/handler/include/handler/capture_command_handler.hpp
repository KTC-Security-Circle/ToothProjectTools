#pragma once

#include "cmd/commands.hpp"

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
///   <bool>: Capture系commandを処理した場合はtrue、それ以外はfalse。
bool handle(runtime::CaptureHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::capture
