#include "dispatch/dispatch.hpp"

#include "runtime/app_context.hpp"
#include "window/window.hpp"

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

    if (local_handler::handle_capture(ctx, dcmd.cmd))
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

            if constexpr (std::is_same_v<T, cmd::TargetAll>)
            {
                ctx.win_mgr.forEach(apply);
            }
            else if constexpr (std::is_same_v<T, cmd::TargetFocus>)
            {
                if (auto* w = ctx.win_mgr.get(ctx.focused_id))
                    apply(*w);
            }
            else if constexpr (std::is_same_v<T, cmd::TargetWindow>)
            {
                if (auto* w = ctx.win_mgr.get(target.window_id))
                    apply(*w);
            }
            else if constexpr (std::is_same_v<T, cmd::TargetCamera>)
            {
                auto it = ctx.cam_to_win.find(target.camera_id);
                if (it != ctx.cam_to_win.end())
                {
                    if (auto* w = ctx.win_mgr.get(it->second))
                        apply(*w);
                }
            }
        },
        dcmd.target);

    ctx.skip_render_once = true;
}

} // namespace dispatch
