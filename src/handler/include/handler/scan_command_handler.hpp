#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace runtime
{
struct ScanHandlerContext;
}

namespace handler::scan
{

common::CommandResult handle(runtime::ScanHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::scan
