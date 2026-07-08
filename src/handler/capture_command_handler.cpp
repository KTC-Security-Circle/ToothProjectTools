#include "handler/capture_command_handler.hpp"

#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"
#include "command_result_mapper/capture_command_result_mapper.hpp"
#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"

#include <type_traits>

namespace handler::capture
{

namespace
{

/// @brief CaptureErrorをlogへ出力する。
///
/// Args:
///   error <const capture::CaptureError&>: log出力するCapture error。
///
/// Return:
///   <void>: 戻り値なし。
void logCaptureError(const ::capture::CaptureError& error)
{
    LOG_ERROR("Capture: failed code={} message={}", ::capture::toString(error.code), error.message);
}

} // namespace

common::CommandResult handle(runtime::CaptureHandlerContext& ctx, const cmd::Command& command)
{
    return std::visit(
        [&](auto&& c) -> common::CommandResult
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdCaptureFrame>)
            {
                const auto capture_result = ctx.capture_service.captureFrame(c.camera_id, c.output_path);
                if (!capture_result.ok && capture_result.error)
                {
                    logCaptureError(*capture_result.error);
                }
                return command_result_mapper::capture::toCommandResult(
                    capture_result,
                    {{"path", capture_result.output_path.string()}});
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCaptureStereo>)
            {
                const auto capture_result = ctx.capture_service.captureStereo(
                    c.left_camera_id, c.right_camera_id, c.left_output_path, c.right_output_path);
                if (!capture_result.ok && capture_result.error)
                {
                    logCaptureError(*capture_result.error);
                }
                return command_result_mapper::capture::toCommandResult(
                    capture_result,
                    {
                        {"left_path", capture_result.left_output_path.string()},
                        {"right_path", capture_result.right_output_path.string()},
                    });
            }
            else
            {
                return common::notHandled();
            }
        },
        command);
}

} // namespace handler::capture
