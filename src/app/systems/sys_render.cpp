#include "app/systems.hpp"

#include <algorithm> // std::max
#include <chrono>

namespace app::sys
{

// -----------------------------------------------------------------------------
// 全ウィンドウの描画 (Present)
// -----------------------------------------------------------------------------
void render_all(runtime::AppContext& ctx)
{
    // コマンド実行直後など、描画を1回スキップしたい場合のフラグ処理
    if (ctx.skip_render_once)
    {
        ctx.skip_render_once = false;
        return;
    }

    const auto now_steady = std::chrono::steady_clock::now();

    // WindowManager管理下のすべてのウィンドウに対して処理
    ctx.win_mgr.forEach(
        [&](win::Window& w)
        {
            // 非表示のウィンドウは更新しない
            if (!w.visible())
                return;

            // リフレッシュレート制御 (簡易的なFPS制限)
            const int fps = w.refreshRate();
            const auto min_delta = std::chrono::nanoseconds(1'000'000'000LL / std::max(1, fps));

            // 前回の描画から十分な時間が経過していれば描画更新
            if (now_steady - w.lastPresented() >= min_delta)
            {
                w.present();
            }
        });
}

} // namespace app::sys