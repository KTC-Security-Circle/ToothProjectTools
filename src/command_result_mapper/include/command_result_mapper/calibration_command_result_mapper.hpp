#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"
#include "service/calibration_service.hpp"

#include <string>

namespace command_result_mapper::calibration
{

/// @brief MonoCalibrationResultをCommandResultへ変換する。
///
/// Args:
///   role <const std::string&>: mono calibration対象のsidecar role。
///   command <const cmd::CmdCalibrate&>: 実行したmono calibration command。
///   result <const service::calibration::MonoCalibrationResult&>: serviceから返されたmono calibration結果。
///
/// Return:
///   <common::CommandResult>: Command Executorで共通利用するcommand実行結果。
common::CommandResult toCommandResult(
    const std::string& role,
    const cmd::CmdCalibrate& command,
    const service::calibration::MonoCalibrationResult& result);

} // namespace command_result_mapper::calibration
