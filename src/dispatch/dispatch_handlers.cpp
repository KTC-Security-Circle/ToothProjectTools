#include "dispatch/dispatch.hpp"
#include "handler/calibration_command_handler.hpp"
#include "handler/capture_command_handler.hpp"
#include "handler/global_command_handler.hpp"
#include "handler/pattern_command_handler.hpp"
#include "handler/reconstruction_command_handler.hpp"
#include "handler/scan_command_handler.hpp"
#include "handler/stereo_calibration_command_handler.hpp"
#include "handler/window_command_handler.hpp"
#include "runtime/handler_context.hpp"

namespace dispatch::local_handler
{

bool handle_global(runtime::AppContext& ctx, const cmd::Command& command)
{
    auto handler_ctx = runtime::make_global_handler_context(ctx);
    return handler::global::handle(handler_ctx, command);
}

bool handle_capture(runtime::AppContext& ctx, const cmd::Command& command)
{
    auto handler_ctx = runtime::make_capture_handler_context(ctx);
    const auto result = handler::capture::handle(handler_ctx, command);
    return result.handled;
}

void handle_window(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    auto window_ctx = runtime::make_window_handler_context(ctx);
    if (handler::window::handle(window_ctx, target_window, command))
        return;

    auto pattern_ctx = runtime::make_pattern_handler_context(ctx);
    if (handler::pattern::handle(pattern_ctx, target_window, command))
        return;

    auto scan_ctx = runtime::make_scan_handler_context(ctx);
    if (handler::scan::handle(scan_ctx, target_window, command))
        return;

    auto calibration_ctx = runtime::make_calibration_handler_context(ctx);
    if (handler::calibration::handle(calibration_ctx, target_window, command))
        return;

    auto stereo_calibration_ctx = runtime::make_stereo_calibration_handler_context(ctx);
    if (handler::stereo_calibration::handle(stereo_calibration_ctx, command))
        return;

    auto reconstruction_ctx = runtime::make_reconstruction_handler_context(ctx);
    if (handler::reconstruction::handle(reconstruction_ctx, target_window, command))
        return;
}

} // namespace local_handler
