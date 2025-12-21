#include "app/systems.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/highgui.hpp>

namespace app::sys {

// -----------------------------------------------------------------------------
// マウスイベントコールバック
// -----------------------------------------------------------------------------
// 引数名 x, y, flags をコメントアウトまたは削除して警告を抑制
void on_mouse_event(int event, int /*x*/, int /*y*/, int /*flags*/, void* userdata) {
    auto* mouse_ctx = static_cast<AppContext::MouseCtx*>(userdata);
    if (!mouse_ctx || !mouse_ctx->ctx) return;

    // 左クリックでフォーカス切り替え
    if (event == cv::EVENT_LBUTTONDOWN) {
        AppContext& ctx = *mouse_ctx->ctx;
        win::WindowId clicked_id = mouse_ctx->wid;

        ctx.focused_id = clicked_id;

        if (auto* w = ctx.win_mgr.get(clicked_id)) {
            LOG_INFO("Mouse: Focus changed to '{}' (id={})", w->name(), clicked_id);
        }
    }
}

// -----------------------------------------------------------------------------
// 入力処理メイン
// -----------------------------------------------------------------------------
void process_input(AppContext& ctx) {
    #if CV_VERSION_MAJOR >= 4 && defined(HAVE_OPENCV_HIGHGUI)
        int key_code = cv::pollKey();
    #else
        int key_code = cv::waitKey(16);
    #endif

    ctx.input.handle(key_code);
}

} // namespace app::sys