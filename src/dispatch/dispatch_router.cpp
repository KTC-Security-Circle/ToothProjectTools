#include "dispatch/dispatch.hpp"
#include "logger/logger_macros.hpp"

namespace dispatch
{

void execute(runtime::AppContext& ctx, const DispatchCmd& dcmd)
{
    // 1. グローバルコマンドかどうかチェック
    // ここで visit して CmdQuit 等なら全体処理、そうでなければターゲット処理へ
    // （簡略化のため、ハンドラ内で分岐させても良いですが、ルーターで分けるのが綺麗です）

    bool handled_globally = false;
    std::visit(
        [&](auto&& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, cmd::CmdQuit>)
            {
                handlers::handle_global(ctx, cmd);
                handled_globally = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdFocusNext>)
            {
                handlers::handle_global(ctx, cmd);
                handled_globally = true;
            }
        },
        dcmd.cmd);

    if (handled_globally)
    {
        ctx.skip_render_once = true;
        return;
    }

    // 2. ターゲットへのディスパッチ
    auto apply = [&](win::Window& w)
    {
        if (w.visible())
            handlers::handle_window(ctx, w, dcmd.cmd);
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