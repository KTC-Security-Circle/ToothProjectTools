#include "control/control_input_adapter.hpp"

#include "control/control_response_adapter.hpp"
#include "control/json_line_writer.hpp"
#include "service/sidecar_service.hpp"

#include <cstdint>
#include <string>

namespace control
{
namespace
{

/// @brief 必須field不足のControlResponseを作成する。
///
/// Args:
///   id <const std::string&>: responseへ設定するrequest id。
///   field <const std::string&>: 不足しているfield名。
///
/// Return:
///   <ControlResponse>: missing_field errorを持つ失敗response。
ControlResponse missingField(const std::string& id, const std::string& field)
{
    return ControlResponse::failure(id, "missing_field", "missing required field: " + field);
}

/// @brief CommandResultから文字列valueを取得する。
///
/// Args:
///   result <const common::CommandResult&>: dispatcherが返したcommand実行結果。
///   key <const std::string&>: 取得するvalues key。
///
/// Return:
///   <std::string>: valuesに存在する文字列。存在しない場合は空文字列。
std::string valueOrEmpty(const common::CommandResult& result, const std::string& key)
{
    const auto it = result.values.find(key);
    return it != result.values.end() ? it->second : std::string{};
}

} // namespace

ControlInputAdapter::ControlInputAdapter(service::SidecarService& service, JsonLineWriter& writer)
    : service_(service), writer_(writer), headless_mapper_(service.cameraService()),
      headless_dispatcher_(service.cameraService(), service.windowService(), service.projectorService(),
                           service.captureService(), service.cameraManager(), service.calibrator(), service.stereoCalibrator(),
                           service.stereoData())
{
}

AdapterResult ControlInputAdapter::handle(const ControlMessage& message)
{
    if (!message.id || message.id->empty())
    {
        writer_.writeResponse(ControlResponse::failure(std::nullopt, "missing_field", "missing required field: id"));
        return AdapterResult::continue_running;
    }
    const auto& id = *message.id;

    if (!message.cmd || message.cmd->empty())
    {
        writer_.writeResponse(missingField(id, "cmd"));
        return AdapterResult::continue_running;
    }

    if (*message.cmd == "ping")
    {
        writer_.writeResponse(ControlResponse::success(id, {{"result", std::string{"pong"}}}));
        return AdapterResult::continue_running;
    }

    if (*message.cmd == "open_camera")
    {
        return handleCameraCommand(id, message, false);
    }

    if (*message.cmd == "close_camera")
    {
        return handleCameraCommand(id, message, true);
    }

    if (*message.cmd == "open_window")
    {
        return handleWindowCommand(id, message, false);
    }

    if (*message.cmd == "close_window")
    {
        return handleWindowCommand(id, message, true);
    }

    if (*message.cmd == "open_projector")
    {
        return handleProjectorCommand(id, message, ProjectorCommandKind::open);
    }

    if (*message.cmd == "close_projector")
    {
        return handleProjectorCommand(id, message, ProjectorCommandKind::close);
    }

    if (*message.cmd == "generate_patterns")
    {
        return handleProjectorCommand(id, message, ProjectorCommandKind::generate);
    }

    if (*message.cmd == "show_pattern")
    {
        return handleProjectorCommand(id, message, ProjectorCommandKind::show);
    }

    if (*message.cmd == "next_pattern")
    {
        return handleProjectorCommand(id, message, ProjectorCommandKind::next);
    }

    if (*message.cmd == "prev_pattern")
    {
        return handleProjectorCommand(id, message, ProjectorCommandKind::prev);
    }

    if (*message.cmd == "start_stream")
    {
        if (!message.role || message.role->empty())
        {
            writer_.writeResponse(missingField(id, "role"));
            return AdapterResult::continue_running;
        }
        const auto result = service_.startStream(*message.role);
        if (!result.ok)
        {
            writeServiceFailure(id, result);
            return AdapterResult::continue_running;
        }
        writer_.writeResponse(ControlResponse::success(id, {{"url", result.value}}));
        writer_.writeEvent(ControlEvent{"stream_started", {{"role", *message.role}, {"url", result.value}}});
        return AdapterResult::continue_running;
    }

    if (*message.cmd == "stop_stream")
    {
        if (!message.role || message.role->empty())
        {
            writer_.writeResponse(missingField(id, "role"));
            return AdapterResult::continue_running;
        }
        const auto result = service_.stopStream(*message.role);
        if (!result.ok)
        {
            writeServiceFailure(id, result);
        }
        else
        {
            writer_.writeResponse(ControlResponse::success(id));
        }
        return AdapterResult::continue_running;
    }

    if (*message.cmd == "capture_frame")
    {
        return handleCaptureFrameCommand(id, message, false);
    }

    if (*message.cmd == "capture_stereo")
    {
        return handleCaptureStereoCommand(id, message, false);
    }

    if (*message.cmd == "calib_capture_frame")
    {
        return handleCaptureFrameCommand(id, message, true);
    }

    if (*message.cmd == "calib_capture_stereo")
    {
        return handleCaptureStereoCommand(id, message, true);
    }

    if (*message.cmd == "mono_calibrate")
    {
        return handleCalibrationCommand(id, message, false);
    }

    if (*message.cmd == "stereo_calibrate")
    {
        return handleCalibrationCommand(id, message, true);
    }

    if (*message.cmd == "shutdown")
    {
        writer_.writeResponse(ControlResponse::success(id));
        return AdapterResult::shutdown;
    }

    // TODO: headless用のDispatchCmd/Handler contextが整った段階で、
    // このcommand mappingを既存dispatch::execute経由へ移行する。
    writer_.writeResponse(ControlResponse::failure(id, "invalid_command", "unknown command: " + *message.cmd));
    return AdapterResult::continue_running;
}

AdapterResult ControlInputAdapter::handleCameraCommand(const std::string& id, const ControlMessage& message, bool close)
{
    const auto map_result = close ? headless_mapper_.mapCloseCamera(message) : headless_mapper_.mapOpenCamera(message);
    if (!map_result.ok)
    {
        writeHeadlessFailure(
            id, map_result.error.value_or(common::CommandError{"invalid_command", "failed to map camera command"}));
        return AdapterResult::continue_running;
    }

    if (close)
    {
        service_.stopStreamIfRunning(*message.role);
    }

    const auto result = headless_dispatcher_.execute(*map_result.command);
    auto response_result = result;
    response_result.values.clear();
    writer_.writeResponse(toControlResponse(id, response_result));
    if (!close && result.handled && result.ok)
    {
        writer_.writeEvent(ControlEvent{
            "camera_opened", {{"camera_id", static_cast<std::int64_t>(*message.camera_id)}, {"role", *message.role}}});
    }
    return AdapterResult::continue_running;
}

AdapterResult ControlInputAdapter::handleWindowCommand(const std::string& id, const ControlMessage& message, bool close)
{
    const auto map_result = close ? headless_mapper_.mapCloseWindow(message) : headless_mapper_.mapOpenWindow(message);
    if (!map_result.ok)
    {
        writeHeadlessFailure(
            id, map_result.error.value_or(common::CommandError{"invalid_command", "failed to map window command"}));
        return AdapterResult::continue_running;
    }

    const auto result = headless_dispatcher_.execute(*map_result.command);
    writer_.writeResponse(toControlResponse(id, result));
    if (result.handled && result.ok)
    {
        if (close)
        {
            writer_.writeEvent(ControlEvent{"window_closed",
                                            {{"window_role", valueOrEmpty(result, "window_role")},
                                             {"window_id", valueOrEmpty(result, "window_id")}}});
        }
        else
        {
            writer_.writeEvent(ControlEvent{"window_opened",
                                            {{"window_role", valueOrEmpty(result, "window_role")},
                                             {"window_id", valueOrEmpty(result, "window_id")},
                                             {"width", valueOrEmpty(result, "width")},
                                             {"height", valueOrEmpty(result, "height")}}});
        }
    }
    return AdapterResult::continue_running;
}


AdapterResult ControlInputAdapter::handleProjectorCommand(const std::string& id, const ControlMessage& message,
                                                          ProjectorCommandKind kind)
{
    headless::CommandMapResult map_result;
    switch (kind)
    {
    case ProjectorCommandKind::open:
        map_result = headless_mapper_.mapOpenProjector(message);
        break;
    case ProjectorCommandKind::close:
        map_result = headless_mapper_.mapCloseProjector(message);
        break;
    case ProjectorCommandKind::generate:
        map_result = headless_mapper_.mapGeneratePatterns(message);
        break;
    case ProjectorCommandKind::show:
        map_result = headless_mapper_.mapProjectorShowPattern(message);
        break;
    case ProjectorCommandKind::next:
        map_result = headless_mapper_.mapProjectorNextPattern(message);
        break;
    case ProjectorCommandKind::prev:
        map_result = headless_mapper_.mapProjectorPrevPattern(message);
        break;
    }

    if (!map_result.ok)
    {
        writeHeadlessFailure(
            id, map_result.error.value_or(common::CommandError{"invalid_command", "failed to map projector command"}));
        return AdapterResult::continue_running;
    }

    const auto result = headless_dispatcher_.execute(*map_result.command);
    writer_.writeResponse(toControlResponse(id, result));
    if (result.handled && result.ok)
    {
        if (kind == ProjectorCommandKind::open)
        {
            writer_.writeEvent(ControlEvent{"projector_opened",
                                            {{"projector_role", valueOrEmpty(result, "projector_role")},
                                             {"window_role", valueOrEmpty(result, "window_role")},
                                             {"width", valueOrEmpty(result, "width")},
                                             {"height", valueOrEmpty(result, "height")}}});
        }
        else if (kind == ProjectorCommandKind::close)
        {
            writer_.writeEvent(ControlEvent{"projector_closed",
                                            {{"projector_role", valueOrEmpty(result, "projector_role")}}});
        }
        else if (kind == ProjectorCommandKind::generate)
        {
            writer_.writeEvent(ControlEvent{"patterns_generated",
                                            {{"projector_role", valueOrEmpty(result, "projector_role")},
                                             {"pattern_count", valueOrEmpty(result, "pattern_count")},
                                             {"width", valueOrEmpty(result, "width")},
                                             {"height", valueOrEmpty(result, "height")}}});
        }
        else
        {
            writer_.writeEvent(ControlEvent{"pattern_shown",
                                            {{"projector_role", valueOrEmpty(result, "projector_role")},
                                             {"pattern_index", valueOrEmpty(result, "pattern_index")}}});
        }
    }
    return AdapterResult::continue_running;
}

AdapterResult ControlInputAdapter::handleCaptureFrameCommand(const std::string& id, const ControlMessage& message,
                                                             bool calibration)
{
    const auto map_result =
        calibration ? headless_mapper_.mapCalibrationCaptureFrame(message) : headless_mapper_.mapCaptureFrame(message);
    if (!map_result.ok)
    {
        writeHeadlessFailure(id, map_result.error.value_or(
                                     common::CommandError{"invalid_command", "failed to map capture_frame command"}));
        return AdapterResult::continue_running;
    }

    auto result = headless_dispatcher_.execute(*map_result.command);
    if (calibration && result.ok)
    {
        result.values.emplace("purpose", "calibration");
    }
    writer_.writeResponse(toControlResponse(id, result));

    const auto path = valueOrEmpty(result, "path");
    if (result.handled && result.ok)
    {
        writer_.writeEvent(ControlEvent{calibration ? "calibration_frame_saved" : "frame_saved",
                                        {{"role", *message.role}, {"path", path}}});
    }
    return AdapterResult::continue_running;
}

AdapterResult ControlInputAdapter::handleCaptureStereoCommand(const std::string& id, const ControlMessage& message,
                                                              bool calibration)
{
    const auto map_result = calibration ? headless_mapper_.mapCalibrationCaptureStereo(message)
                                        : headless_mapper_.mapCaptureStereo(message);
    if (!map_result.ok)
    {
        writeHeadlessFailure(id, map_result.error.value_or(
                                     common::CommandError{"invalid_command", "failed to map capture_stereo command"}));
        return AdapterResult::continue_running;
    }

    auto result = headless_dispatcher_.execute(*map_result.command);
    if (calibration && result.ok)
    {
        result.values.emplace("purpose", "calibration");
    }
    writer_.writeResponse(toControlResponse(id, result));

    const auto left_path = valueOrEmpty(result, "left_path");
    const auto right_path = valueOrEmpty(result, "right_path");
    if (result.handled && result.ok)
    {
        writer_.writeEvent(ControlEvent{calibration ? "calibration_stereo_frame_saved" : "stereo_frame_saved",
                                        {{"left_role", *message.left_role},
                                         {"right_role", *message.right_role},
                                         {"left_path", left_path},
                                         {"right_path", right_path}}});
    }
    return AdapterResult::continue_running;
}

AdapterResult ControlInputAdapter::handleCalibrationCommand(const std::string& id, const ControlMessage& message,
                                                            bool stereo)
{
    const auto map_result =
        stereo ? headless_mapper_.mapStereoCalibrate(message) : headless_mapper_.mapMonoCalibrate(message);
    if (!map_result.ok)
    {
        writeHeadlessFailure(id, map_result.error.value_or(common::CommandError{
                                     "invalid_command", stereo ? "failed to map stereo_calibrate command"
                                                               : "failed to map mono_calibrate command"}));
        return AdapterResult::continue_running;
    }

    const auto result = headless_dispatcher_.execute(*map_result.command);
    writer_.writeResponse(toControlResponse(id, result));

    if (result.handled && result.ok)
    {
        if (stereo)
        {
            writer_.writeEvent(ControlEvent{"stereo_calibration_finished",
                                            {{"left_role", valueOrEmpty(result, "left_role")},
                                             {"right_role", valueOrEmpty(result, "right_role")},
                                             {"output_file", valueOrEmpty(result, "output_file")},
                                             {"rms", valueOrEmpty(result, "rms")}}});
        }
        else
        {
            writer_.writeEvent(ControlEvent{"mono_calibration_finished",
                                            {{"role", valueOrEmpty(result, "role")},
                                             {"output_file", valueOrEmpty(result, "output_file")},
                                             {"rms", valueOrEmpty(result, "rms")}}});
        }
    }
    return AdapterResult::continue_running;
}

void ControlInputAdapter::writeServiceFailure(const std::string& id, const service::SidecarResult& result)
{
    const auto code = result.error ? std::string(service::toString(result.error->code)) : std::string{"internal_error"};

    const auto message =
        result.error ? result.error->message : std::string{"sidecar command failed without error detail"};

    writer_.writeResponse(ControlResponse::failure(id, code, message));
}

void ControlInputAdapter::writeHeadlessFailure(const std::string& id, const common::CommandError& error)
{
    writer_.writeResponse(ControlResponse::failure(id, error.code, error.message));
}

} // namespace control
