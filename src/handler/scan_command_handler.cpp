#include "handler/scan_command_handler.hpp"

#include "logger/logger_macros.hpp"
#include "runtime/app_context.hpp"
#include "window/window.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <type_traits>

namespace fs = std::filesystem;

namespace handler::scan
{

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdStartScan>)
            {
                if (!ctx.sl_system)
                {
                    handled = true;
                    return;
                }

                std::string dir_L = "captures/scan_L";
                std::string dir_R = "captures/scan_R";
                try
                {
                    fs::remove_all(dir_L);
                    fs::remove_all(dir_R);
                    fs::create_directories(dir_L);
                    fs::create_directories(dir_R);
                }
                catch (...)
                {
                    handled = true;
                    return;
                }

                ctx.scan_interval_ms = static_cast<int>(c.interval_ms);
                ctx.sl_system->startScan();
                int count = 0;
                LOG_INFO("=== 自動スキャン開始 (間隔: {}ms) ===", ctx.scan_interval_ms);

                while (true)
                {
                    target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                    target_window.present();
                    cv::waitKey(std::max(50, ctx.scan_interval_ms));

                    cv::Mat img_L, img_R;
                    if (auto* cam = ctx.cam_mgr.get(ctx.scan_cam_id_left))
                    {
                        img_L = cam->getFrame();
                    }
                    if (auto* cam = ctx.cam_mgr.get(ctx.scan_cam_id_right))
                    {
                        img_R = cam->getFrame();
                    }

                    if (auto* w = ctx.win_mgr.get(ctx.id_preview))
                    {
                        if (!img_L.empty())
                        {
                            w->setImage(img_L);
                            w->present();
                        }
                    }
                    if (auto* w = ctx.win_mgr.get(ctx.id_second))
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
                    int prev = ctx.sl_system->getCurrentIndex();
                    ctx.sl_system->nextPattern(false);
                    if (ctx.sl_system->getCurrentIndex() <= prev)
                    {
                        break;
                    }
                }

                ctx.sl_system->stopScan();
                target_window.setImage(
                    cv::Mat(target_window.size().height, target_window.size().width, CV_8UC3, cv::Scalar(0)));
                target_window.present();
                LOG_INFO("=== 自動スキャン完了 (計 {} 枚) ===", count);
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdStopScan>)
            {
                if (ctx.sl_system)
                {
                    ctx.sl_system->stopScan();
                }
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::scan
