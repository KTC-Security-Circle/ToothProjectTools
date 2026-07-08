#pragma once

#include "capture/capture_result.hpp"
#include "common/command_result.hpp"

#include <map>
#include <string>

namespace command_result_mapper::capture
{

/// @brief CaptureResultをCommandResultへ変換する。
///
/// Args:
///   result <const ::capture::CaptureResult&>: CaptureServiceから返されたcapture結果。
///   values <std::map<std::string, std::string>>: command成功時に返す追加値。
///
/// Return:
///   <common::CommandResult>: handler/dispatcherで共通利用するcommand実行結果。
common::CommandResult toCommandResult(
    const ::capture::CaptureResult& result,
    std::map<std::string, std::string> values);

/// @brief CaptureStereoResultをCommandResultへ変換する。
///
/// Args:
///   result <const ::capture::CaptureStereoResult&>: CaptureServiceから返されたstereo capture結果。
///   values <std::map<std::string, std::string>>: command成功時に返す追加値。
///
/// Return:
///   <common::CommandResult>: handler/dispatcherで共通利用するcommand実行結果。
common::CommandResult toCommandResult(
    const ::capture::CaptureStereoResult& result,
    std::map<std::string, std::string> values);

} // namespace command_result_mapper::capture
