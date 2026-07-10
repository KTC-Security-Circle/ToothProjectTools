#include "service/window_service.hpp"

#include "window/window.hpp"
#include "window/window_manager.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <utility>
#include <vector>

namespace service::window
{
namespace
{

/// @brief window roleとして利用可能な文字列か検証する。
///
/// Args:
///   role <const std::string&>: 検証対象role。
///
/// Return:
///   <bool>: 英数字、_、- のみで空でなければtrue。
bool isValidRole(const std::string& role)
{
    if (role.empty())
    {
        return false;
    }
    return std::all_of(role.begin(), role.end(), [](unsigned char ch)
                       { return std::isalnum(ch) || ch == '_' || ch == '-'; });
}

} // namespace

class WindowService::WindowManagerBackend final : public WindowBackend
{
  public:
    /// @brief WindowManager adapterを構築する。
    ///
    /// Args:
    ///   windows <win::WindowManager&>: 実window管理backend。
    ///
    /// Return:
    ///   <WindowManagerBackend>: WindowManager参照を保持するadapter。
    explicit WindowManagerBackend(win::WindowManager& windows) : windows_(windows) {}

    /// @brief WindowManagerでwindowを作成する。
    ///
    /// Args:
    ///   title <const std::string&>: window title。
    ///   width <int>: window幅。
    ///   height <int>: window高さ。
    ///   monitor_index <std::optional<int>>: 表示先monitor index。
    ///   fullscreen <bool>: fullscreenで開くか。
    ///
    /// Return:
    ///   <win::WindowId>: 作成されたwindow id。
    win::WindowId openWindow(const std::string& title, int width, int height,
                             std::optional<int> monitor_index, bool fullscreen) override
    {
        const auto id = windows_.createWindow(title, cv::Size{width, height}, cv::Point{0, 0});
        if (auto* window = windows_.get(id))
        {
            if (monitor_index)
            {
                window->setMonitorIndex(*monitor_index);
            }
            window->resize(win::Size{width, height});
            if (fullscreen)
            {
                window->setFullscreen(true);
            }
        }
        windows_.pollEvents(1);
        return id;
    }

    /// @brief WindowManagerでwindowをcloseする。
    ///
    /// Args:
    ///   window_id <win::WindowId>: close対象window id。
    ///
    /// Return:
    ///   <bool>: close対象が存在しcloseできた場合はtrue。
    bool closeWindow(win::WindowId window_id) override
    {
        const auto closed = windows_.closeWindow(window_id);
        windows_.pollEvents(1);
        return closed;
    }

    /// @brief WindowManagerでwindow event処理を進める。
    ///
    /// Args:
    ///   delay_ms <int>: event pump待機時間ms。
    ///
    /// Return:
    ///   <void>: なし。
    void pollEvents(int delay_ms) override
    {
        windows_.pollEvents(delay_ms);
    }

  private:
    /// windows_ <win::WindowManager&>: 実window管理backend。
    win::WindowManager& windows_;
};

WindowService::WindowService(win::WindowManager& windows)
    : backend_(*(owned_backend_ = std::make_unique<WindowManagerBackend>(windows)))
{
}

WindowService::WindowService(WindowBackend& backend) : backend_(backend)
{
}

WindowService::~WindowService()
{
    closeAll();
}

WindowResult WindowService::openWindow(const WindowOpenConfig& config)
{
    if (!isValidRole(config.role))
    {
        return WindowResult::failure(config.role, "invalid_window_role", "window_role must contain only [A-Za-z0-9_-]");
    }
    if (config.width <= 0 || config.height <= 0)
    {
        return WindowResult::failure(config.role, "invalid_window_size", "width and height must be positive");
    }
    if (config.monitor_index && *config.monitor_index < 0)
    {
        return WindowResult::failure(config.role, "invalid_monitor_index", "monitor_index must be non-negative");
    }
    std::lock_guard lock(mutex_);
    if (role_to_window_id_.contains(config.role))
    {
        return WindowResult::failure(config.role, "window_already_open", "window role is already open: " + config.role);
    }

    const auto title = config.title.empty() ? config.role : config.title;
    try
    {
        const auto window_id = backend_.openWindow(title, config.width, config.height, config.monitor_index,
                                                  config.fullscreen);
        if (window_id == win::kInvalidWindowId)
        {
            return WindowResult::failure(config.role, "window_open_failed", "window backend returned invalid id");
        }
        role_to_window_id_[config.role] = window_id;
        startEventPump();
        return WindowResult::success(config.role, window_id, config.width, config.height);
    }
    catch (const std::exception& error)
    {
        return WindowResult::failure(config.role, "window_open_failed", error.what());
    }
    catch (...)
    {
        return WindowResult::failure(config.role, "internal_error", "window open failed without error detail");
    }
}

WindowResult WindowService::closeWindow(const std::string& role)
{
    win::WindowId window_id{win::kInvalidWindowId};
    bool should_stop_pump{false};

    {
        std::lock_guard lock(mutex_);
        const auto it = role_to_window_id_.find(role);
        if (it == role_to_window_id_.end())
        {
            return WindowResult::failure(role, "window_not_open", "window role is not open: " + role);
        }

        window_id = it->second;
        try
        {
            if (!backend_.closeWindow(window_id))
            {
                return WindowResult::failure(role, "window_close_failed",
                                             "window backend could not close id: " + std::to_string(window_id));
            }
            role_to_window_id_.erase(it);
            should_stop_pump = role_to_window_id_.empty();
        }
        catch (const std::exception& error)
        {
            return WindowResult::failure(role, "window_close_failed", error.what());
        }
        catch (...)
        {
            return WindowResult::failure(role, "internal_error", "window close failed without error detail");
        }
    }

    if (should_stop_pump)
    {
        stopEventPump();
    }
    return WindowResult::success(role, window_id, 0, 0);
}

std::optional<win::WindowId> WindowService::resolveWindowId(const std::string& role) const
{
    std::lock_guard lock(mutex_);
    const auto it = role_to_window_id_.find(role);
    if (it == role_to_window_id_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void WindowService::closeAll()
{
    stopEventPump();

    std::lock_guard lock(mutex_);
    for (const auto& [role, window_id] : role_to_window_id_)
    {
        (void)role;
        (void)backend_.closeWindow(window_id);
    }
    role_to_window_id_.clear();
    backend_.pollEvents(1);
}

void WindowService::pollEvents(int delay_ms)
{
    std::lock_guard lock(mutex_);
    backend_.pollEvents(delay_ms);
}

void WindowService::startEventPump()
{
    if (pump_running_.exchange(true))
    {
        return;
    }

    pump_thread_ = std::thread([this]
                               {
                                   while (pump_running_.load())
                                   {
                                       {
                                           std::lock_guard lock(mutex_);
                                           backend_.pollEvents(1);
                                       }
                                       std::this_thread::sleep_for(std::chrono::milliseconds(16));
                                   }
                               });
}

void WindowService::stopEventPump()
{
    if (!pump_running_.exchange(false))
    {
        return;
    }
    if (pump_thread_.joinable())
    {
        pump_thread_.join();
    }
}

} // namespace service::window
