#pragma once
#include "cmd/commands.hpp"
#include "common/command_result.hpp"
namespace runtime { struct ReconstructionPointCloudHandlerContext; }
namespace handler::reconstruction_point_cloud { common::CommandResult handle(runtime::ReconstructionPointCloudHandlerContext&,const cmd::Command&); }
