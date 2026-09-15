#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace runtime
{
struct MonoCalibrationCalcContext;
}

namespace handler::calibration
{

/// @brief mono calibration計算commandを処理する。
///
/// Args:
///   ctx <runtime::MonoCalibrationCalcContext&>: GUI非依存のmono calibration計算context。
///   command <const cmd::Command&>: dispatch対象のcommand variant。
///
/// Return:
///   <common::CommandResult>: commandの処理有無、成功可否、error、response用values。
common::CommandResult handle(runtime::MonoCalibrationCalcContext& ctx, const cmd::Command& command);

} // namespace handler::calibration
