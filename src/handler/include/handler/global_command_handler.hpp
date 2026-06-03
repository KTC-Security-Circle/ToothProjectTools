#pragma once

#include "cmd/commands.hpp"

namespace runtime
{
struct AppContext;
}

namespace handler::global
{

bool handle(runtime::AppContext& ctx, const cmd::Command& command);

} // namespace handler::global
