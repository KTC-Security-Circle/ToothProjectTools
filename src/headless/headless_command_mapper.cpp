#include "headless/headless_command_mapper.hpp"

#include "control/control_message.hpp"
#include "service/camera_service.hpp"

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
std::optional<CommandMapResult> requireString(const std::optional<std::string>& value, const std::string& field_name)
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
///   camera_service <service::camera::CameraService&>: role bindingを保持するdomain service。
///   role <const std::string&>: 解決対象のcamera role名。
///
/// Return:
///   <std::optional<video::CameraId>>: role登録済みならcamera_id、未登録ならstd::nullopt。
std::optional<video::CameraId> resolveCameraId(service::camera::CameraService& camera_service, const std::string& role)
{
    return camera_service.resolveCameraId(role);
}

/// @brief pathを存在確認なしで比較用に正規化する。
///
/// Args:
///   path <const std::filesystem::path&>: 正規化対象のpath。
///
/// Return:
///   <std::filesystem::path>: absolute化してlexically_normalした比較用path。
std::filesystem::path normalizeOutputPathForCompare(const std::filesystem::path& path)
{
    return std::filesystem::absolute(path).lexically_normal();
}

/// @brief mono calibration output_fileの既定値を作成する。
///
/// Args:
///   role <const std::string&>: mono calibration対象のsidecar role。
///
/// Return:
///   <std::string>: 既定のmono calibration結果file path。
std::string defaultMonoCalibrationOutputFile(const std::string& role)
{
    return "./data/calib/" + role + "_mono.yml";
}

} // namespace

HeadlessCommandMapper::HeadlessCommandMapper(service::camera::CameraService& camera_service)
    : camera_service_(camera_service)
{
}

CommandMapResult HeadlessCommandMapper::mapOpenCamera(const control::ControlMessage& message)
{
    if (!message.camera_id)
    {
        return mapFailure("missing_field", "missing required field: camera_id");
    }
    if (auto failure = requireString(message.role, "role"))
    {
        return *failure;
    }
    if (*message.camera_id < 0)
    {
        return mapFailure("invalid_command", "camera_id must be non-negative");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdOpenCamera{static_cast<video::CameraId>(*message.camera_id), *message.role};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapCloseCamera(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.role, "role"))
    {
        return *failure;
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCloseCamera{*message.role};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapOpenWindow(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.window_role, "window_role"))
    {
        return *failure;
    }
    if (!message.width)
    {
        return mapFailure("missing_field", "missing required field: width");
    }
    if (!message.height)
    {
        return mapFailure("missing_field", "missing required field: height");
    }
    if (*message.width <= 0)
    {
        return mapFailure("invalid_command", "width must be positive");
    }
    if (*message.height <= 0)
    {
        return mapFailure("invalid_command", "height must be positive");
    }
    if (message.monitor_index && *message.monitor_index < 0)
    {
        return mapFailure("invalid_command", "monitor_index must be non-negative");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdOpenWindow{
        *message.window_role,
        message.title.value_or(*message.window_role),
        *message.width,
        *message.height,
        message.monitor_index,
        message.fullscreen.value_or(false),
    };
    return result;
}

CommandMapResult HeadlessCommandMapper::mapCloseWindow(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.window_role, "window_role"))
    {
        return *failure;
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCloseWindow{*message.window_role};
    return result;
}


CommandMapResult HeadlessCommandMapper::mapOpenProjector(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.window_role, "window_role"))
    {
        return *failure;
    }
    if (!message.width)
    {
        return mapFailure("missing_field", "missing required field: width");
    }
    if (!message.height)
    {
        return mapFailure("missing_field", "missing required field: height");
    }
    if (*message.width <= 0)
    {
        return mapFailure("invalid_command", "width must be positive");
    }
    if (*message.height <= 0)
    {
        return mapFailure("invalid_command", "height must be positive");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdOpenProjector{*message.projector_role, *message.window_role, *message.width,
                                           *message.height};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapCloseProjector(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCloseProjector{*message.projector_role};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapGeneratePatterns(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdGeneratePatterns{*message.projector_role};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapProjectorShowPattern(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    if (!message.index)
    {
        return mapFailure("missing_field", "missing required field: index");
    }
    if (*message.index < 0)
    {
        return mapFailure("invalid_command", "index must be non-negative");
    }
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdProjectorShowPattern{*message.projector_role, *message.index};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapProjectorNextPattern(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdProjectorNextPattern{*message.projector_role};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapProjectorPrevPattern(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdProjectorPrevPattern{*message.projector_role};
    return result;
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

    const auto camera_id = resolveCameraId(camera_service_, *message.role);
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

    const auto left_camera_id = resolveCameraId(camera_service_, *message.left_role);
    if (!left_camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.left_role);
    }

    const auto right_camera_id = resolveCameraId(camera_service_, *message.right_role);
    if (!right_camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.right_role);
    }

    if (*left_camera_id == *right_camera_id)
    {
        return mapFailure("invalid_command", "left_role and right_role must resolve to different cameras");
    }

    const auto left_output_path = std::filesystem::path{*message.left_output};
    const auto right_output_path = std::filesystem::path{*message.right_output};
    if (normalizeOutputPathForCompare(left_output_path) == normalizeOutputPathForCompare(right_output_path))
    {
        return mapFailure("invalid_command", "left_output and right_output must be different paths");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCaptureStereo{
        *left_camera_id,
        *right_camera_id,
        left_output_path,
        right_output_path,
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

CommandMapResult HeadlessCommandMapper::mapMonoCalibrate(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.role, "role"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.image_folder, "image_folder"))
    {
        return *failure;
    }

    const auto camera_id = resolveCameraId(camera_service_, *message.role);
    if (!camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.role);
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCalibrate{
        *camera_id,
        *message.image_folder,
        message.output_file.value_or(defaultMonoCalibrationOutputFile(*message.role)),
        *message.role,
    };
    return result;
}

CommandMapResult HeadlessCommandMapper::mapStereoCalibrate(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.left_role, "left_role"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.right_role, "right_role"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.left_dir, "left_dir"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.right_dir, "right_dir"))
    {
        return *failure;
    }

    if (auto failure = requireString(message.output_file, "output_file"))
    {
        return *failure;
    }

    const auto left_camera_id = resolveCameraId(camera_service_, *message.left_role);
    if (!left_camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.left_role);
    }

    const auto right_camera_id = resolveCameraId(camera_service_, *message.right_role);
    if (!right_camera_id)
    {
        return mapFailure("camera_not_open", "role is not opened: " + *message.right_role);
    }

    if (*left_camera_id == *right_camera_id)
    {
        return mapFailure("invalid_command", "left_role and right_role must resolve to different cameras");
    }

    const auto left_dir = std::filesystem::path{*message.left_dir};
    const auto right_dir = std::filesystem::path{*message.right_dir};
    if (normalizeOutputPathForCompare(left_dir) == normalizeOutputPathForCompare(right_dir))
    {
        return mapFailure("invalid_command", "left_dir and right_dir must be different paths");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdStereoCalibrate{
        *left_camera_id,      *right_camera_id,   left_dir.string(),   right_dir.string(),
        *message.output_file, *message.left_role, *message.right_role,
    };
    return result;
}

} // namespace headless
