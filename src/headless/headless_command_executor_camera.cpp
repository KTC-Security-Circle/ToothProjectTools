#include "headless/headless_command_executor.hpp"

#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"
#include "command_result_adapters.hpp"
#include "logger/logger_macros.hpp"
#include "video/camera_service.hpp"

namespace headless
{
namespace
{
void logCaptureError(const capture::CaptureError& error)
{
    LOG_ERROR("Capture: failed code={} message={}", capture::toString(error.code), error.message);
}
} // namespace

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdOpenCamera& command)
{
    return result_adapter::camera(
        camera_service_.openCamera(command.camera_id, command.role), true);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCloseCamera& command)
{
    return result_adapter::camera(camera_service_.closeCamera(command.role), false);
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCaptureFrame& command)
{
    const auto result = capture_service_.captureFrame(command.camera_id, command.output_path);
    if (!result.ok && result.error) logCaptureError(*result.error);
    return result_adapter::capture(result, {{"path", result.output_path.string()}});
}

common::CommandResult HeadlessCommandExecutor::executeTyped(const cmd::CmdCaptureStereo& command)
{
    const auto result = capture_service_.captureStereo(command.left_camera_id, command.right_camera_id,
                                                       command.left_output_path, command.right_output_path);
    if (!result.ok && result.error) logCaptureError(*result.error);
    return result_adapter::capture(
        result, {{"left_path", result.left_output_path.string()},
                 {"right_path", result.right_output_path.string()}});
}
} // namespace headless
