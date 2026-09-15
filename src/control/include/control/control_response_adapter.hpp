#pragma once

#include "common/command_result.hpp"
#include "control/control_message.hpp"

#include <string>

namespace control
{

/// @brief CommandResultをControlResponseへ変換する。
///
/// Args:
///   id <const std::string&>: JSON Lines responseに付与するrequest id。
///   result <const common::CommandResult&>: Command Executorが返したcommand実行結果。
///
/// Return:
///   <ControlResponse>: sidecar stdoutへ出力するresponse。
ControlResponse toControlResponse(
    const std::string& id,
    const common::CommandResult& result);

} // namespace control
