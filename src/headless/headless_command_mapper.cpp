#include "headless/headless_command_mapper.hpp"

#include "control/control_message.hpp"
#include "service/sidecar_service.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace headless
{
namespace
{

/// @brief CommandError付きのCommandMapResult失敗値を作成する。
///
/// Args:
///   code <std::string>: sidecar responseへ返すerror code。
///   message <std::string>: sidecar responseへ返すerror message。
///
/// Return:
///   <CommandMapResult>: command変換失敗を表す結果。
CommandMapResult mapFailure(std::string code, std::string message)
{
    CommandMapResult result;
    result.error = common::CommandError{std::move(code), std::move(message)};
    return result;
}

/// @brief 必須文字列fieldを検証する。
///
/// Args:
///   value <const std::optional<std::string>&>: 検証するfield値。
///   field_name <const std::string&>: error messageへ使用するfield名。
///
/// Return:
///   <std::optional<CommandMapResult>>: field不足時の失敗結果。成功時はstd::nullopt。
std::optional<CommandMapResult> requireString(
    const std::optional<std::string>& value,
    const std::string& field_name)
{
    if (!value || value->empty())
    {
        return mapFailure("missing_field", "missing required field: " + field_name);
    }

    return std::nullopt;
}

/// @brief role名からcamera_idを解決する。
///
/// Args:
///   sidecar_service <service::SidecarService&>: role bindingを保持するsidecar service。
///   role <const std::string&>: 解決対象のcamera role名。
///
/// Return:
///   <std::optional<video::CameraId>>: role登録済みならcamera_id、未登録ならstd::nullopt。
std::optional<video::CameraId> resolveCameraId(
    service::SidecarService& sidecar_service,
    const std::string& role)
{
    return sidecar_service.resolveCameraId(role);
}

} // namespace

HeadlessCommandMapper::HeadlessCommandMapper(service::SidecarService& sidecar_service)
    : sidecar_service_(sidecar_service)
{
}

CommandMapResult HeadlessCommandMapper::mapCaptureFrame(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.role, "role"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.output, "output"))
    {
        return *failure;
    }

    const auto camera_id = resolveCameraId(sidecar_service_, *message.role);
    if (!camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.role);
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCaptureFrame{
        *camera_id,
        std::filesystem::path{*message.output},
    };
    return result;
}

CommandMapResult HeadlessCommandMapper::mapCaptureStereo(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.left_role, "left_role"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.right_role, "right_role"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.left_output, "left_output"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.right_output, "right_output"))
    {
        return *failure;
    }

    const auto left_camera_id = resolveCameraId(sidecar_service_, *message.left_role);
    if (!left_camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.left_role);
    }

    const auto right_camera_id = resolveCameraId(sidecar_service_, *message.right_role);
    if (!right_camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.right_role);
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCaptureStereo{
        *left_camera_id,
        *right_camera_id,
        std::filesystem::path{*message.left_output},
        std::filesystem::path{*message.right_output},
    };
    return result;
}

CommandMapResult HeadlessCommandMapper::mapCalibrationCaptureFrame(const control::ControlMessage& message)
{
    return mapCaptureFrame(message);
}

CommandMapResult HeadlessCommandMapper::mapCalibrationCaptureStereo(const control::ControlMessage& message)
{
    return mapCaptureStereo(message);
}

} // namespace headless
