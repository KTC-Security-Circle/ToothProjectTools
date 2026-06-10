#include "service/scan_service.hpp"

#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"
#include "structured_light/structured_light.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>

namespace fs = std::filesystem;

namespace service::scan
{

void start(runtime::ScanHandlerContext& ctx, win::Window& target_window, const cmd::CmdStartScan& command)
{
    if (!ctx.structured_light)
    {
        return;
    }

    const std::string dir_L = "captures/scan_L";
    const std::string dir_R = "captures/scan_R";
    try
    {
        fs::remove_all(dir_L);
        fs::remove_all(dir_R);
        fs::create_directories(dir_L);
        fs::create_directories(dir_R);
    }
    catch (...)
    {
        return;
    }

    ctx.interval_ms = static_cast<int>(command.interval_ms);
    ctx.structured_light->startScan();
    int count = 0;
    LOG_INFO("=== 自動スキャン開始 (間隔: {}ms) ===", ctx.interval_ms);

    while (true)
    {
        target_window.setImage(ctx.structured_light->getCurrentPatternImage());
        target_window.present();
        cv::waitKey(std::max(50, ctx.interval_ms));

        cv::Mat img_L, img_R;
        if (auto* cam = ctx.cameras.get(ctx.left_camera_id))
        {
            img_L = cam->getFrame();
        }
        if (auto* cam = ctx.cameras.get(ctx.right_camera_id))
        {
            img_R = cam->getFrame();
        }

        if (auto* w = ctx.windows.get(ctx.preview_window_id))
        {
            if (!img_L.empty())
            {
                w->setImage(img_L);
                w->present();
            }
        }
        if (auto* w = ctx.windows.get(ctx.second_window_id))
        {
            if (!img_R.empty())
            {
                w->setImage(img_R);
                w->present();
            }
        }

        cv::waitKey(1);

        if (!img_L.empty() && !img_R.empty())
        {
            std::stringstream ss_L, ss_R;
            ss_L << dir_L << "/" << std::setfill('0') << std::setw(3) << count << ".png";
            ss_R << dir_R << "/" << std::setfill('0') << std::setw(3) << count << ".png";
            cv::imwrite(ss_L.str(), img_L);
            cv::imwrite(ss_R.str(), img_R);
        }

        count++;
        int prev = ctx.structured_light->getCurrentIndex();
        ctx.structured_light->nextPattern(false);
        if (ctx.structured_light->getCurrentIndex() <= prev)
        {
            break;
        }
    }

    ctx.structured_light->stopScan();
    target_window.setImage(cv::Mat(target_window.size().height, target_window.size().width, CV_8UC3, cv::Scalar(0)));
    target_window.present();
    LOG_INFO("=== 自動スキャン完了 (計 {} 枚) ===", count);
}

void stop(runtime::ScanHandlerContext& ctx)
{
    if (ctx.structured_light)
    {
        ctx.structured_light->stopScan();
    }
}

} // namespace service::scan
