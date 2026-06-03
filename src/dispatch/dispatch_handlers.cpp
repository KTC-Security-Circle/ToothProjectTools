#include "dispatch/dispatch.hpp"
#include "handler/calibration_command_handler.hpp"
#include "handler/global_command_handler.hpp"
#include "handler/pattern_command_handler.hpp"
#include "handler/reconstruction_command_handler.hpp"
#include "handler/scan_command_handler.hpp"
#include "handler/stereo_calibration_command_handler.hpp"
#include "handler/window_command_handler.hpp"

namespace dispatch::local_handler
{

bool handle_global(runtime::AppContext& ctx, const cmd::Command& command)
{
    return handler::global::handle(ctx, command);
}

void handle_window(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    if (handler::window::handle(ctx, target_window, command))
        return;
    if (handler::pattern::handle(ctx, target_window, command))
        return;
    if (handler::scan::handle(ctx, target_window, command))
        return;
    if (handler::calibration::handle(ctx, target_window, command))
        return;
    if (handler::stereo_calibration::handle(ctx, target_window, command))
        return;
    if (handler::reconstruction::handle(ctx, target_window, command))
        return;
}

} // namespace local_handler
