#pragma once

#include "common/command_result.hpp"
#include "video/camera_result.hpp"

namespace command_result_mapper::camera
{

/// @brief CameraResultをCommandResultへ変換する。
///
/// Args:
///   result <const video::CameraResult&>: camera serviceの実行結果。
///   include_camera_id <bool>: success valuesへcamera_idを含めるか。
///
/// Return:
///   <common::CommandResult>: Command Executorで共通利用するcommand実行結果。
common::CommandResult toCommandResult(const video::CameraResult& result, bool include_camera_id = true);

} // namespace command_result_mapper::camera
