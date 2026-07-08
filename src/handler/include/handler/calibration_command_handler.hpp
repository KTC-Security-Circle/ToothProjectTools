#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace runtime
{
struct CalibrationHandlerContext;
struct MonoCalibrationCalcContext;
}

namespace win
{
class Window;
}

namespace handler::calibration
{

/// @brief Calibration系GUI commandを処理する。
///
/// Args:
///   ctx <runtime::CalibrationHandlerContext&>: GUI calibration handler context。
///   target_window <win::Window&>: command対象window。
///   command <const cmd::Command&>: dispatch対象のcommand variant。
///
/// Return:
///   <common::CommandResult>: commandの処理有無、成功可否、error、response用values。
common::CommandResult handle(runtime::CalibrationHandlerContext& ctx, win::Window& target_window, const cmd::Command& command);

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
