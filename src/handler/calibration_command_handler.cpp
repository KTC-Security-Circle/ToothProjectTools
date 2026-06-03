#include "handler/calibration_command_handler.hpp"

#include "logger/logger_macros.hpp"
#include "runtime/app_context.hpp"
#include "video/camera.hpp"
#include "window/window.hpp"

#include <filesystem>
#include <iomanip>
#include <iterator>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <type_traits>
#include <vector>

namespace fs = std::filesystem;

namespace handler::calibration
{

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdCalibClear>)
            {
                if (target_window.id() == ctx.id_preview && !c.target_directory.empty())
                {
                    fs::remove_all(c.target_directory);
                    fs::create_directories(c.target_directory);
                    LOG_INFO("Calib: フォルダクリア {}", c.target_directory);
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCalibCapture>)
            {
                if (c.camera_id == video::kInvalidCameraId)
                {
                    handled = true;
                    return;
                }

                auto it = ctx.cam_to_win.find(c.camera_id);
                if (it != ctx.cam_to_win.end() && it->second != target_window.id())
                {
                    handled = true;
                    return;
                }

                if (!fs::exists(c.target_directory))
                {
                    fs::create_directories(c.target_directory);
                }

                auto cnt = std::distance(fs::directory_iterator(c.target_directory), fs::directory_iterator{});
                if (auto* cam = ctx.cam_mgr.get(c.camera_id))
                {
                    cv::Mat frame = cam->getFrame();
                    if (!frame.empty())
                    {
                        std::stringstream ss;
                        ss << c.target_directory << "/" << c.prefix << std::setfill('0') << std::setw(3) << cnt
                           << ".png";
                        cv::imwrite(ss.str(), frame);
                        LOG_INFO("Saved[{}]: {}", cam->name(), ss.str());
                    }
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCalibrate>)
            {
                if (!ctx.calibrator)
                {
                    handled = true;
                    return;
                }

                std::vector<std::string> files;
                if (fs::exists(c.image_folder))
                {
                    for (auto& p : fs::directory_iterator(c.image_folder))
                    {
                        if (p.is_regular_file())
                        {
                            files.push_back(p.path().string());
                        }
                    }
                }

                if (files.empty())
                {
                    handled = true;
                    return;
                }

                cv::Mat K, D;
                double rms = ctx.calibrator->runCalibration(files, K, D);
                if (rms > 0 && rms < 1.0)
                {
                    if (auto* cam = ctx.cam_mgr.get(c.target_camera_id))
                    {
                        cam->setIntrinsics(K);
                        cam->setDistCoeffs(D);
                    }
                    LOG_INFO("Calib: 成功 RMS={}", rms);
                }
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::calibration
