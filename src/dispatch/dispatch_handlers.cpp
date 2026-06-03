// =============================================================================
//  src/dispatch/dispatch_handlers.cpp
// =============================================================================

#include "dispatch/dispatch.hpp"
#include "logger/logger_macros.hpp"
#include "dispatch/logic/reconstruct.hpp"
#include "video/camera.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace dispatch::handlers
{

// -----------------------------------------------------------------------------
// グローバルコマンド
// -----------------------------------------------------------------------------
bool handle_global(runtime::AppContext& ctx, const cmd::Command& command)
{
    bool handled = false;
    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, cmd::CmdFocusNext>)
            {
                std::vector<win::WindowId> ids;
                ctx.win_mgr.forEach(
                    [&](win::Window& w)
                    {
                        if (w.visible())
                            ids.push_back(w.id());
                    });
                if (!ids.empty())
                {
                    std::sort(ids.begin(), ids.end());
                    auto it = std::find(ids.begin(), ids.end(), ctx.focused_id);
                    ctx.focused_id = (it == ids.end() || std::next(it) == ids.end()) ? ids[0] : *std::next(it);
                }
                handled = true;
            }
            else if constexpr (std::is_same_v<T, cmd::CmdQuit>)
            {
                ctx.running = false;
                handled = true;
            }
        },
        command);
    return handled;
}

// -----------------------------------------------------------------------------
// ウィンドウレベルコマンド
// -----------------------------------------------------------------------------
void handle_window(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    std::visit(
        [&](auto&& c)
        {
            using T = std::decay_t<decltype(c)>;

            // 表示系
            if constexpr (std::is_same_v<T, cmd::CmdToggleFullscreen>)
            {
                target_window.setFullscreen(!target_window.fullscreen());
            }
            else if constexpr (std::is_same_v<T, cmd::CmdMoveToMonitor>)
            {
                target_window.setMonitorIndex(c.index);
            }

            // パターン制御
            else if constexpr (std::is_same_v<T, cmd::CmdShowPattern>)
            {
                if (ctx.sl_system)
                {
                    ctx.sl_system->setIndex(c.index);
                    target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                }
            }
            else if constexpr (std::is_same_v<T, cmd::CmdNextPattern>)
            {
                if (ctx.sl_system)
                {
                    ctx.sl_system->nextPattern(true);
                    target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                }
            }
            else if constexpr (std::is_same_v<T, cmd::CmdPrevPattern>)
            {
                if (ctx.sl_system)
                {
                    ctx.sl_system->prevPattern(true);
                    target_window.setImage(ctx.sl_system->getCurrentPatternImage());
                }
            }

            // スキャン制御
            else if constexpr (std::is_same_v<T, cmd::CmdStartScan>)
            {
                if (!ctx.sl_system)
                    return;
                std::string dir_L = "captures/scan_L", dir_R = "captures/scan_R";
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
                        img_L = cam->getFrame();
                    if (auto* cam = ctx.cam_mgr.get(ctx.scan_cam_id_right))
                        img_R = cam->getFrame();
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
                        break;
                }
                ctx.sl_system->stopScan();
                target_window.setImage(
                    cv::Mat(target_window.size().height, target_window.size().width, CV_8UC3, cv::Scalar(0)));
                target_window.present();
                LOG_INFO("=== 自動スキャン完了 (計 {} 枚) ===", count);
            }
            else if constexpr (std::is_same_v<T, cmd::CmdStopScan>)
            {
                if (ctx.sl_system)
                    ctx.sl_system->stopScan();
            }

            // キャリブレーション支援
            else if constexpr (std::is_same_v<T, cmd::CmdCalibClear>)
            {
                if (target_window.id() == ctx.id_preview && !c.target_directory.empty())
                {
                    fs::remove_all(c.target_directory);
                    fs::create_directories(c.target_directory);
                    LOG_INFO("Calib: フォルダクリア {}", c.target_directory);
                }
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCalibCapture>)
            {
                if (c.camera_id == video::kInvalidCameraId)
                    return;
                auto it = ctx.cam_to_win.find(c.camera_id);
                if (it != ctx.cam_to_win.end() && it->second != target_window.id())
                    return;
                if (!fs::exists(c.target_directory))
                    fs::create_directories(c.target_directory);
                auto cnt = std::distance(fs::directory_iterator(c.target_directory), {});
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
            }
            else if constexpr (std::is_same_v<T, cmd::CmdCalibrate>)
            {
                if (!ctx.calibrator)
                    return;
                std::vector<std::string> files;
                if (fs::exists(c.image_folder))
                    for (auto& p : fs::directory_iterator(c.image_folder))
                        if (p.is_regular_file())
                            files.push_back(p.path().string());
                if (files.empty())
                    return;
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
            }
            else if constexpr (std::is_same_v<T, cmd::CmdStereoCalibrate>)
            {
                if (!ctx.stereo_calibrator)
                    return;
                auto* cL = ctx.cam_mgr.get(c.left_cam_id);
                auto* cR = ctx.cam_mgr.get(c.right_cam_id);
                if (!cL || !cR)
                    return;
                cv::Mat K1 = cL->intrinsics(), D1 = cL->distCoeffs(), K2 = cR->intrinsics(), D2 = cR->distCoeffs();
                if (K1.empty() || K2.empty())
                {
                    LOG_ERROR("Stereo: 単眼パラメータ不足");
                    return;
                }

                auto getf = [](std::string d)
                {
                    std::vector<std::string> f;
                    if (fs::exists(d))
                        for (auto& e : fs::directory_iterator(d))
                            if (e.is_regular_file())
                                f.push_back(e.path().string());
                    std::sort(f.begin(), f.end());
                    return f;
                };
                auto fL = getf(c.left_dir);
                auto fR = getf(c.right_dir);
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
                    cv::FileStorage fs(c.output_file, cv::FileStorage::WRITE);
                    if (fs.isOpened())
                    {
                        fs << "RMS" << rms << "K1" << K1 << "D1" << D1 << "K2" << K2 << "D2" << D2 << "R" << res.R
                           << "T" << res.T << "Q" << res.Q;
                    }
                }
            }
            // =====================================================================
            // 3D復元 (ロジックを分離)
            // =====================================================================
            else if constexpr (std::is_same_v<T, cmd::CmdReconstruct>)
            {
                dispatch::logic::run_reconstruction(ctx, c, target_window);
            }
        },
        command);
}

} // namespace dispatch::handlers
