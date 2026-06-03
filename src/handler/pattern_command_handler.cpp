#include "handler/pattern_command_handler.hpp"

#include "runtime/app_context.hpp"
#include "window/window.hpp"

#include <type_traits>

namespace handler::pattern
{

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdShowPattern>)
            {
                if (ctx.sl_system)
                {
                    ctx.sl_system->setIndex(c.index);
                    target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdNextPattern>)
            {
                if (ctx.sl_system)
                {
                    ctx.sl_system->nextPattern(true);
                    target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdPrevPattern>)
            {
                if (ctx.sl_system)
                {
                    ctx.sl_system->prevPattern(true);
                    target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                }
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::pattern
