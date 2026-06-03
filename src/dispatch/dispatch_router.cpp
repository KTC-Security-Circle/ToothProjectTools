#include "dispatch/dispatch.hpp"
#include "logger/logger_macros.hpp"

#include <type_traits>

namespace dispatch
{

void execute(runtime::AppContext& ctx, const DispatchCmd& dcmd)
{
    if (local_handler::handle_global(ctx, dcmd.cmd))
    {
        ctx.skip_render_once = true;
        return;
    }

    auto apply = [&](win::Window& w)
    {
        if (w.visible())
            local_handler::handle_window(ctx, w, dcmd.cmd);
    };

    std::visit(
        [&](auto&& target)
        {
            using T = std::decay_t<decltype(target)>;

            if constexpr (std::is_same_v<T, TargetAll>)
            {
                ctx.win_mgr.forEach(apply);
            }
            else if constexpr (std::is_same_v<T, TargetFocused>)
            {
                if (auto* w = ctx.win_mgr.get(ctx.focused_id))
                    apply(*w);
            }
            else if constexpr (std::is_same_v<T, TargetById>)
            {
                if (auto* w = ctx.win_mgr.get(target.id))
                    apply(*w);
            }
        },
        dcmd.target);

    ctx.skip_render_once = true;
}

} // namespace dispatch
