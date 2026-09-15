#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

namespace runtime
{
struct StereoCalibrationCalcContext;
}

namespace handler::stereo_calibration
{

/// @brief stereo calibration計算commandを処理する。
///
/// Args:
///   ctx <runtime::StereoCalibrationCalcContext&>: GUI非依存のstereo calibration計算context。
///   command <const cmd::Command&>: dispatch対象のcommand variant。
///
/// Return:
///   <common::CommandResult>: commandの処理有無、成功可否、error、response用values。
common::CommandResult handle(runtime::StereoCalibrationCalcContext& ctx, const cmd::Command& command);

} // namespace handler::stereo_calibration
