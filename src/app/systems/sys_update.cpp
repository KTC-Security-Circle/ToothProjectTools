#include "app/systems.hpp"
#include "dispatch/dispatch.hpp" // saveScanResultsなどをコマンド化するか、ここで実装するか
#include "logger/logger_macros.hpp"
#include "video/camera.hpp"

#include <chrono>
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

namespace
{

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (const auto ch : value)
    {
        if (ch == '\\' || ch == '"')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

std::string nowIsoLike()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return stream.str();
}

std::string patternFileName(std::size_t index)
{
    std::ostringstream name;
    name << "pattern_" << std::setw(3) << std::setfill('0') << index << ".png";
    return name.str();
}

void logSaveFailed(const char* reason, const fs::path& output_dir)
{
    LOG_WARN("Scan: 保存失敗 reason={} output_dir={}", reason, output_dir.string());
}

} // namespace

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
    const int expected = ctx.scan_pattern_count > 0 ? ctx.scan_pattern_count
                                                    : (ctx.sl_system ? ctx.sl_system->getPatternCount() : 0);

    if (!ctx.sl_system)
    {
        logSaveFailed("missing_sl_system", ctx.scan_output_dir);
        return;
    }

    if (expected <= 0 ||
        ctx.scanned_imgs_left.size() != static_cast<std::size_t>(expected) ||
        ctx.scanned_imgs_right.size() != static_cast<std::size_t>(expected) ||
        ctx.scanned_imgs_left.size() != ctx.scanned_imgs_right.size())
    {
        LOG_WARN("Scan: incomplete dataset; skip saving metadata/output. expected={} left_count={} right_count={}",
                 expected,
                 ctx.scanned_imgs_left.size(),
                 ctx.scanned_imgs_right.size());
        logSaveFailed("incomplete_dataset", ctx.scan_output_dir);
        return;
    }

    for (int i = 0; i < expected; ++i)
    {
        if (ctx.scanned_imgs_left[static_cast<std::size_t>(i)].empty() ||
            ctx.scanned_imgs_right[static_cast<std::size_t>(i)].empty())
        {
            LOG_WARN("Scan: incomplete dataset; empty image at pattern {} left_empty={} right_empty={}",
                     i,
                     ctx.scanned_imgs_left[static_cast<std::size_t>(i)].empty(),
                     ctx.scanned_imgs_right[static_cast<std::size_t>(i)].empty());
            logSaveFailed("empty_frame", ctx.scan_output_dir);
            return;
        }
    }

    if (ctx.scan_id.empty() || ctx.scan_surface.surface_width <= 0 || ctx.scan_surface.surface_height <= 0 ||
        ctx.scan_surface.pattern_width <= 0 || ctx.scan_surface.pattern_height <= 0)
    {
        logSaveFailed("invalid_metadata", ctx.scan_output_dir);
        return;
    }

    const fs::path left_dir = ctx.scan_output_dir / "left";
    const fs::path right_dir = ctx.scan_output_dir / "right";
    const fs::path metadata_path = ctx.scan_output_dir / "metadata.json";

    try
    {
        fs::create_directories(left_dir);
        fs::create_directories(right_dir);

        std::error_code remove_error;
        fs::remove(metadata_path, remove_error);
        if (remove_error)
        {
            LOG_WARN("Scan: failed to remove stale metadata path={} error={}", metadata_path.string(), remove_error.message());
        }

        std::size_t left_saved_count = 0;
        std::size_t right_saved_count = 0;

        auto save_images = [](const std::vector<cv::Mat>& images, const fs::path& dir, std::size_t& saved_count)
        {
            for (std::size_t i = 0; i < images.size(); ++i)
            {
                const fs::path output_path = dir / patternFileName(i);
                if (!cv::imwrite(output_path.string(), images[i]))
                {
                    LOG_WARN("Scan: failed to save {}", output_path.string());
                    return false;
                }
                ++saved_count;
            }
            return true;
        };

        if (!save_images(ctx.scanned_imgs_left, left_dir, left_saved_count) ||
            !save_images(ctx.scanned_imgs_right, right_dir, right_saved_count) ||
            left_saved_count != static_cast<std::size_t>(expected) ||
            right_saved_count != static_cast<std::size_t>(expected))
        {
            logSaveFailed("image_write_failed", ctx.scan_output_dir);
            return;
        }

        std::ofstream metadata(metadata_path);
        if (!metadata)
        {
            logSaveFailed("metadata_open_failed", ctx.scan_output_dir);
            return;
        }

        metadata << "{\n"
                 << "  \"scan_id\": \"" << jsonEscape(ctx.scan_id) << "\",\n"
                 << "  \"created_at\": \"" << jsonEscape(nowIsoLike()) << "\",\n"
                 << "  \"version\": \"0.1.0\",\n"
                 << "  \"projector_role\": \"" << jsonEscape(ctx.scan_projector_role) << "\",\n"
                 << "  \"left_role\": \"" << jsonEscape(ctx.scan_left_role) << "\",\n"
                 << "  \"right_role\": \"" << jsonEscape(ctx.scan_right_role) << "\",\n"
                 << "  \"pattern_count\": " << expected << ",\n"
                 << "  \"settle_ms\": " << ctx.scan_interval_ms << ",\n"
                 << "  \"output_dir\": \"" << jsonEscape(ctx.scan_output_dir.string()) << "\",\n"
                 << "  \"surface\": {\n"
                 << "    \"monitor_index\": " << ctx.scan_surface.monitor_index << ",\n"
                 << "    \"monitor_width\": " << ctx.scan_surface.monitor_width << ",\n"
                 << "    \"monitor_height\": " << ctx.scan_surface.monitor_height << ",\n"
                 << "    \"surface_width\": " << ctx.scan_surface.surface_width << ",\n"
                 << "    \"surface_height\": " << ctx.scan_surface.surface_height << ",\n"
                 << "    \"pattern_width\": " << ctx.scan_surface.pattern_width << ",\n"
                 << "    \"pattern_height\": " << ctx.scan_surface.pattern_height << ",\n"
                 << "    \"pattern_x\": " << ctx.scan_surface.pattern_x << ",\n"
                 << "    \"pattern_y\": " << ctx.scan_surface.pattern_y << ",\n"
                 << "    \"clamped\": " << (ctx.scan_surface.clamped ? "true" : "false") << "\n"
                 << "  }\n"
                 << "}\n";

        if (!metadata)
        {
            logSaveFailed("metadata_write_failed", ctx.scan_output_dir);
            return;
        }

        LOG_INFO("Scan: 保存完了 scan_id={} output_dir={} pattern_count={} left_saved={} right_saved={}",
                 ctx.scan_id,
                 ctx.scan_output_dir.string(),
                 expected,
                 left_saved_count,
                 right_saved_count);
    }
    catch (const std::exception& e)
    {
        LOG_WARN("Scan: 保存失敗 reason={} output_dir={}", e.what(), ctx.scan_output_dir.string());
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
        auto capture_frame = [&](video::CameraId cid) -> cv::Mat
        {
            if (cid == video::kInvalidCameraId)
            {
                return {};
            }

            if (auto* c = ctx.cam_mgr.get(cid))
            {
                if (!c->isOpened())
                {
                    return {};
                }
                return c->getFrame();
            }

            return {};
        };

        cv::Mat left = capture_frame(ctx.scan_cam_id_left);
        cv::Mat right = capture_frame(ctx.scan_cam_id_right);

        if (left.empty() || right.empty())
        {
            LOG_WARN("Scan: capture failed at pattern {} left_empty={} right_empty={}",
                     ctx.sl_system->getCurrentIndex(),
                     left.empty(),
                     right.empty());
            ctx.sl_system->stopScan();
            return;
        }

        ctx.scanned_imgs_left.push_back(left.clone());
        ctx.scanned_imgs_right.push_back(right.clone());

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
