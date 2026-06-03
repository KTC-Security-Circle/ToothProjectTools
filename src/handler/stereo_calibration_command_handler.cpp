#include "handler/stereo_calibration_command_handler.hpp"

#include "calibration/stereo_data.hpp"
#include "logger/logger_macros.hpp"
#include "runtime/app_context.hpp"
#include "video/camera.hpp"
#include "window/window.hpp"

#include <algorithm>
#include <filesystem>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <type_traits>
#include <vector>

namespace fs = std::filesystem;

namespace handler::stereo_calibration
{

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    (void)target_window;

    bool handled = false;

    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, cmd::CmdStereoCalibrate>)
            {
                if (!ctx.stereo_calibrator)
                {
                    handled = true;
                    return;
                }

                auto* cL = ctx.cam_mgr.get(c.left_cam_id);
                auto* cR = ctx.cam_mgr.get(c.right_cam_id);
                if (!cL || !cR)
                {
                    handled = true;
                    return;
                }

                cv::Mat K1 = cL->intrinsics();
                cv::Mat D1 = cL->distCoeffs();
                cv::Mat K2 = cR->intrinsics();
                cv::Mat D2 = cR->distCoeffs();
                if (K1.empty() || K2.empty())
                {
                    LOG_ERROR("Stereo: 単眼パラメータ不足");
                    handled = true;
                    return;
                }

                auto getf = [](const std::string& d)
                {
                    std::vector<std::string> f;
                    if (fs::exists(d))
                    {
                        for (auto& e : fs::directory_iterator(d))
                        {
                            if (e.is_regular_file())
                            {
                                f.push_back(e.path().string());
                            }
                        }
                    }
                    std::sort(f.begin(), f.end());
                    return f;
                };

                auto fL = getf(c.left_dir);
                auto fR = getf(c.right_dir);
                if (fL.size() != fR.size() || fL.empty())
                {
                    LOG_ERROR("Stereo: 画像数不一致");
                    handled = true;
                    return;
                }

                LOG_INFO("Stereo: 計算開始 {} pairs", fL.size());
                calib::StereoData res;
                double rms = ctx.stereo_calibrator->run(fL, fR, K1, D1, K2, D2, res);
                if (rms > 0 && res.valid)
                {
                    LOG_INFO("Stereo: 成功! RMS={}", rms);
                    ctx.stereo_data = res;
                    cv::FileStorage fs_out(c.output_file, cv::FileStorage::WRITE);
                    if (fs_out.isOpened())
                    {
                        fs_out << "RMS" << rms << "K1" << K1 << "D1" << D1 << "K2" << K2 << "D2" << D2 << "R" << res.R
                               << "T" << res.T << "Q" << res.Q;
                    }
                }
                handled = true;
            }
        },
        command);

    return handled;
}

} // namespace handler::stereo_calibration
