#include "handler/window_command_handler.hpp"

#include "runtime/app_context.hpp"
#include "window/window.hpp"

#include <type_traits>

namespace handler::window
{

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    (void)ctx;

    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdToggleFullscreen>)
            {
                target_window.setFullscreen(!target_window.fullscreen());
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdMoveToMonitor>)
            {
                target_window.setMonitorIndex(c.index);
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::window
