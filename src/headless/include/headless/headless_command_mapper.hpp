#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

#include <optional>

namespace control
{
struct ControlMessage;
}

namespace service
{
class SidecarService;
}

namespace headless
{

struct CommandMapResult
{
    /// ok <bool>: ControlMessageからcmd::Commandへの変換に成功したか。
    bool ok{false};

    /// command <std::optional<cmd::Command>>: 変換成功時のcommand。
    std::optional<cmd::Command> command;

    /// error <std::optional<common::CommandError>>: 変換失敗時のerror情報。
    std::optional<common::CommandError> error;
};

class HeadlessCommandMapper
{
  public:
    /// @brief SidecarServiceのrole bindingを参照してHeadlessCommandMapperを構築する。
    ///
    /// Args:
    ///   sidecar_service <service::SidecarService&>: roleからcamera_idを解決するsidecar service。
    ///
    /// Return:
    ///   <HeadlessCommandMapper>: SidecarService参照を保持するmapper。
    explicit HeadlessCommandMapper(service::SidecarService& sidecar_service);

    /// @brief capture_frame用ControlMessageをCmdCaptureFrameへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCaptureFrame(const control::ControlMessage& message);

  private:
    /// sidecar_service_ <service::SidecarService&>: role bindingを保持するsidecar service。
    service::SidecarService& sidecar_service_;
};

} // namespace headless
