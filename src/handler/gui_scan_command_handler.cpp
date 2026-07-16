#include "handler/gui_scan_command_handler.hpp"

#include "logger/logger_macros.hpp"
#include "runtime/app_context.hpp"
#include "structured_light/structured_light.hpp"
#include "video/camera.hpp"
#include "window/window.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <variant>

namespace handler::gui_scan
{

namespace
{

std::string nowCompact()
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
    stream << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return stream.str();
}

std::string generateGuiScanId()
{
    return "scan_" + nowCompact();
}

bool handle_gui_scan_start(runtime::AppContext& ctx, win::Window& projector_window, const cmd::CmdStartScan& command)
{
    if (!ctx.sl_system)
    {
        LOG_WARN("Scan: StructuredLight system is not available");
        return true;
    }

    if (ctx.sl_system->isScanning())
    {
        LOG_WARN("Scan: already running");
        return true;
    }

    auto* left_camera = ctx.cam_mgr.get(ctx.scan_cam_id_left);
    auto* right_camera = ctx.cam_mgr.get(ctx.scan_cam_id_right);
    if (ctx.scan_cam_id_left == video::kInvalidCameraId ||
        ctx.scan_cam_id_right == video::kInvalidCameraId ||
        !left_camera ||
        !right_camera ||
        !left_camera->isOpened() ||
        !right_camera->isOpened())
    {
        LOG_WARN("Scan: cannot start because left/right cameras are not ready. "
                 "left_id={} right_id={} left_exists={} right_exists={} left_open={} right_open={}",
                 ctx.scan_cam_id_left,
                 ctx.scan_cam_id_right,
                 left_camera != nullptr,
                 right_camera != nullptr,
                 left_camera ? left_camera->isOpened() : false,
                 right_camera ? right_camera->isOpened() : false);
        return true;
    }

    const auto projector_size = projector_window.size();
    const auto monitor_size = projector_window.getMonitorSize();
    const int surface_width = projector_size.width;
    const int surface_height = projector_size.height;
    const int pattern_width = projector_size.width;
    const int pattern_height = projector_size.height;
    const int pattern_count = ctx.sl_system->getPatternCount();

    if (surface_width <= 0 || surface_height <= 0 || pattern_width <= 0 || pattern_height <= 0 || pattern_count <= 0)
    {
        LOG_WARN("Scan: cannot start because projector surface is invalid. "
                 "surface_width={} surface_height={} pattern_width={} pattern_height={} pattern_count={}",
                 surface_width,
                 surface_height,
                 pattern_width,
                 pattern_height,
                 pattern_count);
        return true;
    }

    ctx.scanned_imgs_left.clear();
    ctx.scanned_imgs_right.clear();
    ctx.scan_id = (command.scan_id && !command.scan_id->empty()) ? command.scan_id.value() : generateGuiScanId();
    ctx.scan_projector_role = command.projector_role;
    ctx.scan_left_role = command.left_role;
    ctx.scan_right_role = command.right_role;
    ctx.scan_output_dir = command.output_dir.empty() ? std::filesystem::path{"./data/scan/default"}
                                                     : std::filesystem::path{command.output_dir};
    ctx.scan_interval_ms = command.settle_ms > 0 ? command.settle_ms : 500;
    ctx.scan_pattern_count = pattern_count;
    ctx.scan_surface.monitor_index = projector_window.monitorIndex();
    ctx.scan_surface.monitor_width = monitor_size.width > 0 ? monitor_size.width : surface_width;
    ctx.scan_surface.monitor_height = monitor_size.height > 0 ? monitor_size.height : surface_height;
    ctx.scan_surface.surface_width = surface_width;
    ctx.scan_surface.surface_height = surface_height;
    ctx.scan_surface.pattern_width = pattern_width;
    ctx.scan_surface.pattern_height = pattern_height;
    ctx.scan_surface.pattern_x = 0;
    ctx.scan_surface.pattern_y = 0;
    ctx.scan_surface.clamped = false;

    ctx.sl_system->setIndex(0);
    ctx.sl_system->startScan();

    projector_window.setImage(ctx.sl_system->getCurrentPatternImage());

    LOG_INFO("Scan: started scan_id={} interval_ms={} pattern_count={} output_dir={}",
             ctx.scan_id,
             ctx.scan_interval_ms,
             ctx.scan_pattern_count,
             ctx.scan_output_dir.string());

    return true;
}

bool handle_gui_scan_stop(runtime::AppContext& ctx)
{
    if (!ctx.sl_system)
    {
        LOG_WARN("Scan: StructuredLight system is not available");
        return true;
    }

    if (!ctx.sl_system->isScanning())
    {
        LOG_INFO("Scan: stop requested but scan is not running");
        return true;
    }

    ctx.sl_system->stopScan();
    LOG_INFO("Scan: stopped");
    return true;
}

} // namespace

bool handle(runtime::AppContext& ctx, win::Window& target_window, const cmd::Command& command)
{
    bool handled = false;

    std::visit(
        [&](const auto& scan_command)
        {
            using CommandType = std::decay_t<decltype(scan_command)>;

            if constexpr (std::is_same_v<CommandType, cmd::CmdStartScan>)
            {
                handled = handle_gui_scan_start(ctx, target_window, scan_command);
            }
            else if constexpr (std::is_same_v<CommandType, cmd::CmdStopScan>)
            {
                handled = handle_gui_scan_stop(ctx);
            }
        },
        command);

    return handled;
}

} // namespace handler::gui_scan
