#include "service/stereo_calibration_service.hpp"

#include "calibration/stereo_calibrator.hpp"
#include "calibration/stereo_data.hpp"
#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"
#include "video/camera.hpp"
#include "video/camera_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <opencv2/core.hpp>
#include <vector>

namespace fs = std::filesystem;

namespace service::stereo_calibration
{

void calibrate(runtime::StereoCalibrationHandlerContext& ctx, const cmd::CmdStereoCalibrate& command)
{
    if (!ctx.stereo_calibrator)
    {
        return;
    }

    auto* cL = ctx.cameras.get(command.left_cam_id);
    auto* cR = ctx.cameras.get(command.right_cam_id);
    if (!cL || !cR)
    {
        return;
    }

    cv::Mat K1 = cL->intrinsics();
    cv::Mat D1 = cL->distCoeffs();
    cv::Mat K2 = cR->intrinsics();
    cv::Mat D2 = cR->distCoeffs();
    if (K1.empty() || K2.empty())
    {
        LOG_ERROR("Stereo: 単眼パラメータ不足");
        return;
    }

    auto getf = [](const std::string& dir)
    {
        std::vector<std::string> files;
        if (fs::exists(dir))
        {
            for (auto& e : fs::directory_iterator(dir))
            {
                if (e.is_regular_file())
                {
                    files.push_back(e.path().string());
                }
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    };

    auto fL = getf(command.left_dir);
    auto fR = getf(command.right_dir);
    if (fL.size() != fR.size() || fL.empty())
    {
        LOG_ERROR("Stereo: 画像数不一致");
        return;
    }

    LOG_INFO("Stereo: 計算開始 {} pairs", fL.size());
    calib::StereoData res;
    double rms = ctx.stereo_calibrator->run(fL, fR, K1, D1, K2, D2, res);
    if (rms > 0 && res.valid)
    {
        LOG_INFO("Stereo: 成功! RMS={}", rms);
        ctx.stereo_data = res;
        cv::FileStorage fs_out(command.output_file, cv::FileStorage::WRITE);
        if (fs_out.isOpened())
        {
            fs_out << "RMS" << rms << "K1" << K1 << "D1" << D1 << "K2" << K2 << "D2" << D2 << "R" << res.R
                   << "T" << res.T << "Q" << res.Q;
        }
    }
}

} // namespace service::stereo_calibration
