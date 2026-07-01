#include "handler/capture_command_handler.hpp"

#include "capture/capture_result.hpp"
#include "capture/capture_service.hpp"
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

bool handle(runtime::CaptureHandlerContext& ctx, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdCaptureFrame>)
            {
                const auto result = ctx.capture_service.captureFrame(c.camera_id, c.output_path);
                if (!result.ok && result.error)
                {
                    logCaptureError(*result.error);
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCaptureStereo>)
            {
                const auto result = ctx.capture_service.captureStereo(
                    c.left_camera_id, c.right_camera_id, c.left_output_path, c.right_output_path);
                if (!result.ok && result.error)
                {
                    logCaptureError(*result.error);
                }
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::capture
