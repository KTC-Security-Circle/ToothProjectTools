#pragma once

#include "headless/headless_command_result.hpp"

#include "cmd/commands.hpp"

namespace capture
{
class CaptureService;
}

namespace headless
{

class HeadlessDispatcher
{
  public:
    /// @brief CaptureServiceを参照してHeadlessDispatcherを構築する。
    ///
    /// Args:
    ///   capture_service <capture::CaptureService&>: GUIに依存しないcapture commandを実行するdomain service。
    ///
    /// Return:
    ///   <HeadlessDispatcher>: CaptureService参照を保持するheadless dispatcher。
    explicit HeadlessDispatcher(capture::CaptureService& capture_service);

    /// @brief headlessで実行可能なcommandを実行する。
    ///
    /// Args:
    ///   command <const cmd::Command&>: 実行対象のcommand variant。
    ///
    /// Return:
    ///   <HeadlessCommandResult>: commandの処理有無、成功可否、error、response用values。
    HeadlessCommandResult execute(const cmd::Command& command);

  private:
    /// capture_service_ <capture::CaptureService&>: capture系domain commandを実行するservice。
    capture::CaptureService& capture_service_;
};

} // namespace headless
