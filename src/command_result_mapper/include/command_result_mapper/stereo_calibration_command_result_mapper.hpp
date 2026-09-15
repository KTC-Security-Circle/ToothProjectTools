#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"
#include "service/stereo_calibration_service.hpp"

#include <string>

namespace command_result_mapper::stereo_calibration
{

/// @brief StereoCalibrationResultをCommandResultへ変換する。
///
/// Args:
///   left_role <const std::string&>: 左cameraのsidecar role。
///   right_role <const std::string&>: 右cameraのsidecar role。
///   command <const cmd::CmdStereoCalibrate&>: 実行したstereo calibration command。
///   result <const service::stereo_calibration::StereoCalibrationResult&>: serviceから返されたstereo calibration結果。
///
/// Return:
///   <common::CommandResult>: Command Executorで共通利用するcommand実行結果。
common::CommandResult toCommandResult(
    const std::string& left_role,
    const std::string& right_role,
    const cmd::CmdStereoCalibrate& command,
    const service::stereo_calibration::StereoCalibrationResult& result);

} // namespace command_result_mapper::stereo_calibration
