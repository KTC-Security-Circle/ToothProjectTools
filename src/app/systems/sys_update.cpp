#include "app/systems.hpp"
#include "dispatch/dispatch.hpp" // saveScanResultsなどをコマンド化するか、ここで実装するか
#include "logger/logger_macros.hpp"
#include "video/camera.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>

namespace fs = std::filesystem;

namespace app::sys
{

// -----------------------------------------------------------------------------
// コマンド処理
// -----------------------------------------------------------------------------
void process_commands(runtime::AppContext& ctx)
{
    while (!ctx.cmd_que.empty())
    {
        const auto& dcmd = ctx.cmd_que.front();
        dispatch::execute(ctx, dcmd);
        ctx.cmd_que.pop_front();
        if (!ctx.running)
            break;
    }
}

// -----------------------------------------------------------------------------
// プレビュー更新
// -----------------------------------------------------------------------------
void update_preview(runtime::AppContext& ctx)
{
    ctx.cam_mgr.forEach(
        [&](video::Camera& cam)
        {
            if (!cam.isOpened())
                return;
            cv::Mat frame = cam.getFrame();
            if (frame.empty())
                return;

            cv::Mat display = frame;
            bool is_scanning = (ctx.sl_system && ctx.sl_system->isScanning());

            if (ctx.calibrator && ctx.show_chess_corners && !is_scanning)
            {
                cv::Mat vis;
                std::vector<cv::Point2f> corners;
                ctx.calibrator->detectAndDraw(frame, vis, corners);
                if (!vis.empty())
                    display = vis;
            }

            if (ctx.cam_to_win.count(cam.id()))
            {
                auto wid = ctx.cam_to_win[cam.id()];
                if (auto* w = ctx.win_mgr.get(wid))
                    w->setImage(display);
            }
        });
}

// -----------------------------------------------------------------------------
// スキャンロジック (保存ヘルパー含む)
// -----------------------------------------------------------------------------
static void save_scan_results(runtime::AppContext& ctx)
{
    if (ctx.scanned_imgs_left.empty() && ctx.scanned_imgs_right.empty())
        return;

    const fs::path left_dir = ctx.scan_output_dir / "left";
    const fs::path right_dir = ctx.scan_output_dir / "right";

    try
    {
        fs::create_directories(left_dir);
        fs::create_directories(right_dir);

        auto save_images = [](const std::vector<cv::Mat>& images, const fs::path& dir, const char* side)
        {
            for (std::size_t i = 0; i < images.size(); ++i)
            {
                if (images[i].empty())
                {
                    LOG_WARN("Scan: {} image {} is empty; skip save", side, i);
                    continue;
                }

                std::ostringstream name;
                name << "pattern_" << std::setw(3) << std::setfill('0') << i << ".png";
                const fs::path output_path = dir / name.str();
                if (!cv::imwrite(output_path.string(), images[i]))
                    LOG_WARN("Scan: failed to save {}", output_path.string());
            }
        };

        save_images(ctx.scanned_imgs_left, left_dir, "left");
        save_images(ctx.scanned_imgs_right, right_dir, "right");

        std::ofstream metadata(ctx.scan_output_dir / "metadata.json");
        metadata << "{\n"
                 << "  \"output_dir\": \"" << ctx.scan_output_dir.string() << "\",\n"
                 << "  \"left_count\": " << ctx.scanned_imgs_left.size() << ",\n"
                 << "  \"right_count\": " << ctx.scanned_imgs_right.size() << ",\n"
                 << "  \"pattern_count\": " << (ctx.sl_system ? ctx.sl_system->getPatternCount() : 0) << "\n"
                 << "}\n";

        LOG_INFO("Scan: 保存完了 output_dir={} left_count={} right_count={}",
                 ctx.scan_output_dir.string(),
                 ctx.scanned_imgs_left.size(),
                 ctx.scanned_imgs_right.size());
    }
    catch (const std::exception& e)
    {
        LOG_WARN("Scan: 保存失敗 output_dir={} error={}", ctx.scan_output_dir.string(), e.what());
    }
}

void update_scan(runtime::AppContext& ctx)
{
    if (!ctx.sl_system || !ctx.sl_system->isScanning())
        return;

    LOG_INFO("Scan: update tick current_index={} left_count={} right_count={}",
             ctx.sl_system->getCurrentIndex(),
             ctx.scanned_imgs_left.size(),
             ctx.scanned_imgs_right.size());

    if (ctx.sl_system->checkTimerAndReset(ctx.scan_interval_ms))
    {
        // 撮影
        auto capture = [&](video::CameraId cid, std::vector<cv::Mat>& buf)
        {
            cv::Mat f;
            if (cid != video::kInvalidCameraId)
            {
                if (auto* c = ctx.cam_mgr.get(cid))
                    f = c->getFrame();
            }
            buf.push_back(f.empty() ? cv::Mat() : f.clone());
        };

        capture(ctx.scan_cam_id_left, ctx.scanned_imgs_left);
        capture(ctx.scan_cam_id_right, ctx.scanned_imgs_right);

        LOG_INFO("Scan: Pattern {}", ctx.sl_system->getCurrentIndex());

        // 次へ
        int old_idx = ctx.sl_system->getCurrentIndex();
        ctx.sl_system->nextPattern(false);

        if (ctx.sl_system->getCurrentIndex() > old_idx)
        {
            // 投影
            if (auto* p = ctx.win_mgr.get(ctx.id_projector))
                p->setImage(ctx.sl_system->getCurrentPatternImage());
        }
        else
        {
            // 完了
            ctx.sl_system->stopScan();
            if (auto* p = ctx.win_mgr.get(ctx.id_projector))
            {
                // 黒画面
                cv::Mat blk(p->size().height, p->size().width, CV_8UC3, cv::Scalar(0, 0, 0));
                p->setImage(blk);
            }
            save_scan_results(ctx);
        }
    }
}

} // namespace app::sys