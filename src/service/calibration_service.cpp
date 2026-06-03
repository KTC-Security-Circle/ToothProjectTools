#include "service/calibration_service.hpp"

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
#include <vector>

namespace fs = std::filesystem;

namespace service::calibration
{

void clear(runtime::AppContext& ctx, win::Window& target_window, const cmd::CmdCalibClear& command)
{
    if (target_window.id() == ctx.id_preview && !command.target_directory.empty())
    {
        fs::remove_all(command.target_directory);
        fs::create_directories(command.target_directory);
        LOG_INFO("Calib: フォルダクリア {}", command.target_directory);
    }
}

void capture(runtime::AppContext& ctx, win::Window& target_window, const cmd::CmdCalibCapture& command)
{
    if (command.camera_id == video::kInvalidCameraId)
    {
        return;
    }

    auto it = ctx.cam_to_win.find(command.camera_id);
    if (it != ctx.cam_to_win.end() && it->second != target_window.id())
    {
        return;
    }

    if (!fs::exists(command.target_directory))
    {
        fs::create_directories(command.target_directory);
    }

    auto cnt = std::distance(fs::directory_iterator(command.target_directory), fs::directory_iterator{});
    if (auto* cam = ctx.cam_mgr.get(command.camera_id))
    {
        cv::Mat frame = cam->getFrame();
        if (!frame.empty())
        {
            std::stringstream ss;
            ss << command.target_directory << "/" << command.prefix << std::setfill('0') << std::setw(3) << cnt
               << ".png";
            cv::imwrite(ss.str(), frame);
            LOG_INFO("Saved[{}]: {}", cam->name(), ss.str());
        }
    }
}

void calibrate(runtime::AppContext& ctx, const cmd::CmdCalibrate& command)
{
    if (!ctx.calibrator)
    {
        return;
    }

    std::vector<std::string> files;
    if (fs::exists(command.image_folder))
    {
        for (auto& p : fs::directory_iterator(command.image_folder))
        {
            if (p.is_regular_file())
            {
                files.push_back(p.path().string());
            }
        }
    }

    if (files.empty())
    {
        return;
    }

    cv::Mat K, D;
    double rms = ctx.calibrator->runCalibration(files, K, D);
    if (rms > 0 && rms < 1.0)
    {
        if (auto* cam = ctx.cam_mgr.get(command.target_camera_id))
        {
            cam->setIntrinsics(K);
            cam->setDistCoeffs(D);
        }
        LOG_INFO("Calib: 成功 RMS={}", rms);
    }
}

} // namespace service::calibration
