#include "handler/pattern_command_handler.hpp"

#include "runtime/handler_context.hpp"
#include "structured_light/structured_light.hpp"
#include "window/window.hpp"

#include <type_traits>

namespace handler::pattern
{

bool handle(runtime::PatternHandlerContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdShowPattern>)
            {
                if (ctx.structured_light)
                {
                    ctx.structured_light->setIndex(c.index);
                    target_window.setImage(ctx.structured_light->getCurrentPatternImage());
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdNextPattern>)
            {
                if (ctx.structured_light)
                {
                    ctx.structured_light->nextPattern(true);
                    target_window.setImage(ctx.structured_light->getCurrentPatternImage());
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdPrevPattern>)
            {
                if (ctx.structured_light)
                {
                    ctx.structured_light->prevPattern(true);
                    target_window.setImage(ctx.structured_light->getCurrentPatternImage());
                }
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::pattern
