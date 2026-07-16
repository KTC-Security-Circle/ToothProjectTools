#include "handler/gui_scan_command_handler.hpp"

#include "logger/logger_macros.hpp"
#include "runtime/app_context.hpp"
#include "structured_light/structured_light.hpp"
#include "window/window.hpp"

#include <filesystem>
#include <type_traits>
#include <variant>

namespace handler::gui_scan
{

namespace
{

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

    ctx.scanned_imgs_left.clear();
    ctx.scanned_imgs_right.clear();
    ctx.scan_output_dir = command.output_dir.empty() ? std::filesystem::path{"./data/scan/default"}
                                                     : std::filesystem::path{command.output_dir};

    ctx.scan_interval_ms = command.settle_ms > 0 ? command.settle_ms : 500;

    ctx.sl_system->setIndex(0);
    ctx.sl_system->startScan();

    projector_window.setImage(ctx.sl_system->getCurrentPatternImage());

    LOG_INFO("Scan: started interval_ms={} pattern_count={} output_dir={}",
             ctx.scan_interval_ms,
             ctx.sl_system->getPatternCount(),
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
