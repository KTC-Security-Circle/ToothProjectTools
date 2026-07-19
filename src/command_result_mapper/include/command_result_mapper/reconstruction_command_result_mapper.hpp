#pragma once
#include "common/command_result.hpp"
#include "reconstruction/reconstruction_service.hpp"
namespace command_result_mapper::reconstruction { common::CommandResult toCommandResult(const ::reconstruction::ReconstructionValidationResult&); common::CommandResult toCommandResult(const ::reconstruction::ReconstructionResult&); }
