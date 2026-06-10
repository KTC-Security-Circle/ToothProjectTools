#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct GlobalHandlerContext;
}

namespace handler::global
{

bool handle(runtime::GlobalHandlerContext& ctx, const cmd::Command& command);

} // namespace handler::global
