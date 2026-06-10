#include "handler/global_command_handler.hpp"

#include "runtime/handler_context.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <vector>

namespace handler::global
{

bool handle(runtime::GlobalHandlerContext& ctx, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdFocusNext>)
            {
                std::vector<win::WindowId> ids;
                ctx.windows.forEach(
                    [&](win::Window& w)
                    {
                        if (w.visible())
                            ids.push_back(w.id());
                    });

                if (!ids.empty())
                {
                    std::sort(ids.begin(), ids.end());
                    auto it = std::find(ids.begin(), ids.end(), ctx.focused_id);
                    ctx.focused_id = (it == ids.end() || std::next(it) == ids.end()) ? ids[0] : *std::next(it);
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdQuit>)
            {
                ctx.running = false;
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::global
