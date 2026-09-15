#pragma once

#include "common/command_result.hpp"
#include "scan/scan_result.hpp"

namespace command_result_mapper::scan
{

common::CommandResult toCommandResult(const ::scan::ScanResult& result);

} // namespace command_result_mapper::scan
