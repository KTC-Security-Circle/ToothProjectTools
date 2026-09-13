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



CommandMapResult HeadlessCommandMapper::mapListMonitors(const control::ControlMessage& message)
{
    (void)message;
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdListMonitors{};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapConfigureProjectorSurface(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    if (!message.monitor_index)
    {
        return mapFailure("missing_field", "missing required field: monitor_index");
    }
    if (!message.width)
    {
        return mapFailure("missing_field", "missing required field: width");
    }
    if (!message.height)
    {
        return mapFailure("missing_field", "missing required field: height");
    }
    if (*message.monitor_index < 0)
    {
        return mapFailure("invalid_command", "monitor_index must be non-negative");
    }
    if (*message.width <= 0)
    {
        return mapFailure("invalid_command", "width must be positive");
    }
    if (*message.height <= 0)
    {
        return mapFailure("invalid_command", "height must be positive");
    }

    const auto placement = message.placement.value_or("center");
    if (placement != "center" && placement != "custom")
    {
        return mapFailure("invalid_command", "placement must be center or custom");
    }
    if (placement == "custom" && !message.x)
    {
        return mapFailure("missing_field", "missing required field: x");
    }
    if (placement == "custom" && !message.y)
    {
        return mapFailure("missing_field", "missing required field: y");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdConfigureProjectorSurface{*message.projector_role, *message.monitor_index,
                                                       *message.width, *message.height, message.x, message.y,
                                                       placement};
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


CommandMapResult HeadlessCommandMapper::mapStartScan(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.projector_role, "projector_role"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.left_role, "left_role"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.output_dir, "output_dir"))
    {
        return *failure;
    }
    if (message.scan_id && message.scan_id->empty())
    {
        return mapFailure("invalid_command", "scan_id must not be empty");
    }
    if (message.settle_ms && *message.settle_ms < 0)
    {
        return mapFailure("invalid_command", "settle_ms must be non-negative");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdStartScan{message.scan_id,
                                       *message.projector_role,
                                       *message.left_role,
                                       message.right_role.value_or(std::string{}),
                                       *message.output_dir,
                                       message.settle_ms.value_or(120),
                                       message.sync_source.value_or("fixed_delay"),
                                       message.sync_timeout_ms.value_or(1000), message.sync_guard_ms.value_or(30),
                                       message.sync_stable_frames.value_or(3), message.roi_x.value_or(0),
                                       message.roi_y.value_or(0), message.roi_width.value_or(32),
                                       message.roi_height.value_or(32), message.roi_black_threshold.value_or(40),
                                       message.roi_white_threshold.value_or(180)};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapScanStatus(const control::ControlMessage& message)
{
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdScanStatus{message.scan_id};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapStopScan(const control::ControlMessage& message)
{
    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdStopScan{message.scan_id};
    return result;
}

CommandMapResult HeadlessCommandMapper::mapValidateScanDataset(const control::ControlMessage& message)
{
    const bool has_input_dir = message.input_dir && !message.input_dir->empty();
    const bool has_left_dir = message.left_dir && !message.left_dir->empty();
    const bool has_right_dir = message.right_dir && !message.right_dir->empty();
    if (!has_input_dir && !has_left_dir && !has_right_dir)
    {
        return mapFailure("missing_field", "missing required field: input_dir or left_dir/right_dir");
    }
    if ((message.input_dir && message.input_dir->empty()) || (message.left_dir && message.left_dir->empty()) ||
        (message.right_dir && message.right_dir->empty()))
    {
        return mapFailure("invalid_command", "input_dir/left_dir/right_dir must not be empty");
    }
    if (has_left_dir != has_right_dir)
    {
        return mapFailure("missing_field", has_left_dir ? "missing required field: right_dir" : "missing required field: left_dir");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdValidateScanDataset{
        message.input_dir.value_or(std::string{}),
        message.allow_partial.value_or(false),
        message.left_dir.value_or(std::string{}),
        message.right_dir.value_or(std::string{}),
        message.metadata_file.value_or(std::string{}),
    };
    return result;
}

CommandMapResult HeadlessCommandMapper::mapDecodePatterns(const control::ControlMessage& message)
{
    const bool has_input_dir = message.input_dir && !message.input_dir->empty();
    const bool has_left_dir = message.left_dir && !message.left_dir->empty();
    const bool has_right_dir = message.right_dir && !message.right_dir->empty();
    if (!has_input_dir && !has_left_dir && !has_right_dir)
    {
        return mapFailure("missing_field", "missing required field: input_dir or left_dir/right_dir");
    }
    if (!message.output_dir)
    {
        return mapFailure("missing_field", "missing required field: output_dir");
    }
    if ((message.input_dir && message.input_dir->empty()) || (message.left_dir && message.left_dir->empty()) ||
        (message.right_dir && message.right_dir->empty()) || message.output_dir->empty())
    {
        return mapFailure("invalid_command", "input_dir/left_dir/right_dir/output_dir must not be empty");
    }
    if (has_left_dir != has_right_dir)
    {
        return mapFailure("missing_field", has_left_dir ? "missing required field: right_dir" : "missing required field: left_dir");
    }
    if (message.threshold && *message.threshold < 0)
    {
        return mapFailure("invalid_command", "threshold must be non-negative");
    }
    if (message.projector_width && *message.projector_width <= 0)
    {
        return mapFailure("invalid_command", "projector_width must be positive");
    }
    if (message.projector_height && *message.projector_height <= 0)
    {
        return mapFailure("invalid_command", "projector_height must be positive");
    }
    if (message.pattern_count && *message.pattern_count <= 0)
    {
        return mapFailure("invalid_command", "pattern_count must be positive");
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdDecodePatterns{
        message.input_dir.value_or(std::string{}),
        *message.output_dir,
        message.threshold.value_or(15),
        message.allow_partial.value_or(false),
        message.left_dir.value_or(std::string{}),
        message.right_dir.value_or(std::string{}),
        message.metadata_file.value_or(std::string{}),
        message.projector_width,
        message.projector_height,
        message.pattern_count,
    };
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
    if (auto failure = requireString(message.image_folder, "image_folder"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.output_file, "output_file"))
    {
        return *failure;
    }

    const bool apply_to_camera = message.apply_to_camera.value_or(false);
    video::CameraId camera_id = video::kInvalidCameraId;
    if (apply_to_camera)
    {
        if (auto failure = requireString(message.role, "role"))
        {
            return *failure;
        }
        const auto resolved_camera_id = resolveCameraId(camera_service_, *message.role);
        if (!resolved_camera_id)
        {
            return mapFailure("camera_not_open", "role is not opened: " + *message.role);
        }
        camera_id = *resolved_camera_id;
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdCalibrate{
        camera_id,
        *message.image_folder,
        *message.output_file,
        message.role.value_or(std::string{}),
        apply_to_camera,
    };
    return result;
}

CommandMapResult HeadlessCommandMapper::mapStereoCalibrate(const control::ControlMessage& message)
{
    if (auto failure = requireString(message.left_dir, "left_dir"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.right_dir, "right_dir"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.left_calibration_file, "left_calibration_file"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.right_calibration_file, "right_calibration_file"))
    {
        return *failure;
    }
    if (auto failure = requireString(message.output_file, "output_file"))
    {
        return *failure;
    }

    const auto left_dir = std::filesystem::path{*message.left_dir};
    const auto right_dir = std::filesystem::path{*message.right_dir};
    if (normalizeOutputPathForCompare(left_dir) == normalizeOutputPathForCompare(right_dir))
    {
        return mapFailure("invalid_command", "left_dir and right_dir must be different paths");
    }

    const bool apply_to_camera = message.apply_to_camera.value_or(false);
    video::CameraId left_camera_id = video::kInvalidCameraId;
    video::CameraId right_camera_id = video::kInvalidCameraId;
    if (apply_to_camera)
    {
        if (auto failure = requireString(message.left_role, "left_role"))
        {
            return *failure;
        }
        if (auto failure = requireString(message.right_role, "right_role"))
        {
            return *failure;
        }
        const auto resolved_left_camera_id = resolveCameraId(camera_service_, *message.left_role);
        if (!resolved_left_camera_id)
        {
            return mapFailure("camera_not_open", "role is not opened: " + *message.left_role);
        }
        const auto resolved_right_camera_id = resolveCameraId(camera_service_, *message.right_role);
        if (!resolved_right_camera_id)
        {
            return mapFailure("camera_not_open", "role is not opened: " + *message.right_role);
        }
        if (*resolved_left_camera_id == *resolved_right_camera_id)
        {
            return mapFailure("invalid_command", "left_role and right_role must resolve to different cameras");
        }
        left_camera_id = *resolved_left_camera_id;
        right_camera_id = *resolved_right_camera_id;
    }

    CommandMapResult result;
    result.ok = true;
    result.command = cmd::CmdStereoCalibrate{
        left_camera_id,
        right_camera_id,
        left_dir.string(),
        right_dir.string(),
        *message.output_file,
        message.left_role.value_or(std::string{}),
        message.right_role.value_or(std::string{}),
        *message.left_calibration_file,
        *message.right_calibration_file,
        apply_to_camera,
    };
    return result;
}

} // namespace headless

namespace headless {
CommandMapResult HeadlessCommandMapper::mapValidateReconstruction(const control::ControlMessage& m) { if(!m.decode_dir||m.decode_dir->empty()) return mapFailure("missing_field","missing required field: decode_dir"); if(!m.calibration_file||m.calibration_file->empty()) return mapFailure("missing_field","missing required field: calibration_file"); const double e=m.max_epipolar_error_px.value_or(2.0); if(!(e>0.0)) return mapFailure("invalid_command","max_epipolar_error_px must be positive"); if(m.min_depth_mm&&*m.min_depth_mm<=0) return mapFailure("invalid_command","min_depth_mm must be positive"); if(m.max_depth_mm&&*m.max_depth_mm<=0) return mapFailure("invalid_command","max_depth_mm must be positive"); if(m.min_depth_mm&&m.max_depth_mm&&*m.min_depth_mm>=*m.max_depth_mm) return mapFailure("invalid_command","min_depth_mm must be less than max_depth_mm"); CommandMapResult r;r.ok=true;r.command=cmd::CmdValidateReconstruction{*m.decode_dir,*m.calibration_file,{e,m.min_depth_mm,m.max_depth_mm}};return r; }
CommandMapResult HeadlessCommandMapper::mapReconstructPointCloud(const control::ControlMessage& m) { auto r=mapValidateReconstruction(m); if(!r.ok)return r; if(!m.output_file||m.output_file->empty())return mapFailure("missing_field","missing required field: output_file");auto v=std::get<cmd::CmdValidateReconstruction>(*r.command);r.command=cmd::CmdReconstructPointCloud{v.decode_dir,v.calibration_file,*m.output_file,v.config,m.overwrite.value_or(false)};return r; }
}
