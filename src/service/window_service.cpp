#include "service/window_service.hpp"

#include "logger/logger_macros.hpp"
#include "window/window.hpp"
#include "window/window_manager.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <sstream>
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

/// @brief OpenCV HighGUI backend名を取得する。
///
/// Args:
///   なし。
///
/// Return:
///   <std::string>: backend名。取得APIがないOpenCVではunavailable。
std::string highGuiBackendName()
{
    return "unavailable";
}

/// @brief thread idをlog用文字列へ変換する。
///
/// Args:
///   id <std::thread::id>: 変換対象thread id。
///
/// Return:
///   <std::string>: log出力用thread id文字列。
std::string threadIdToString(std::thread::id id)
{
    std::ostringstream stream;
    stream << id;
    return stream.str();
}

} // namespace

struct WindowService::WindowRequest
{
    /// @brief request種別。
    enum class Kind
    {
        Open,
        Close,
        CloseAll,
    };

    /// kind <Kind>: main threadで実行するrequest種別。
    Kind kind;

    /// promise <std::promise<WindowResult>>: 呼び出し元へ返すrequest実行結果。
    std::promise<WindowResult> promise;

    /// @brief WindowRequestを構築する。
    ///
    /// Args:
    ///   request_kind <Kind>: request種別。
    ///
    /// Return:
    ///   <WindowRequest>: request基底。
    explicit WindowRequest(Kind request_kind) : kind(request_kind) {}

    /// @brief requestを破棄する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <void>: なし。
    virtual ~WindowRequest() = default;
};

struct WindowService::OpenWindowRequest final : WindowRequest
{
    /// config <WindowOpenConfig>: window作成設定。
    WindowOpenConfig config;

    /// @brief open window requestを構築する。
    ///
    /// Args:
    ///   open_config <WindowOpenConfig>: window作成設定。
    ///
    /// Return:
    ///   <OpenWindowRequest>: open request。
    explicit OpenWindowRequest(WindowOpenConfig open_config)
        : WindowRequest(Kind::Open), config(std::move(open_config))
    {
    }
};

struct WindowService::CloseWindowRequest final : WindowRequest
{
    /// role <std::string>: close対象window role名。
    std::string role;

    /// @brief close window requestを構築する。
    ///
    /// Args:
    ///   close_role <std::string>: close対象window role名。
    ///
    /// Return:
    ///   <CloseWindowRequest>: close request。
    explicit CloseWindowRequest(std::string close_role) : WindowRequest(Kind::Close), role(std::move(close_role)) {}
};

struct WindowService::CloseAllWindowsRequest final : WindowRequest
{
    /// @brief close all windows requestを構築する。
    ///
    /// Args:
    ///   なし。
    ///
    /// Return:
    ///   <CloseAllWindowsRequest>: close all request。
    CloseAllWindowsRequest() : WindowRequest(Kind::CloseAll) {}
};

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
        return windows_.closeWindow(window_id);
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
    : backend_(*(owned_backend_ = std::make_unique<WindowManagerBackend>(windows))),
      gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
}

WindowService::WindowService(WindowBackend& backend) : backend_(backend), gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
}

WindowService::~WindowService() = default;

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

    auto request = std::make_shared<OpenWindowRequest>(config);
    auto future = request->promise.get_future();
    enqueue(request);
    return future.get();
}

WindowResult WindowService::closeWindow(const std::string& role)
{
    auto request = std::make_shared<CloseWindowRequest>(role);
    auto future = request->promise.get_future();
    enqueue(request);
    return future.get();
}

std::optional<win::WindowId> WindowService::resolveWindowId(const std::string& role) const
{
    if (std::this_thread::get_id() != gui_thread_id_)
    {
        LOG_DEBUG("resolveWindowId called from non-GUI thread; role={}", role);
    }

    const auto it = role_to_window_id_.find(role);
    if (it == role_to_window_id_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void WindowService::closeAll()
{
    auto request = std::make_shared<CloseAllWindowsRequest>();
    auto future = request->promise.get_future();
    enqueue(request);
    (void)future.get();
}

void WindowService::processPendingRequests()
{
    if (!ensureGuiThread("processPendingRequests"))
    {
        return;
    }

    std::deque<std::shared_ptr<WindowRequest>> pending;
    {
        std::lock_guard lock(queue_mutex_);
        pending.swap(requests_);
    }

    for (const auto& request : pending)
    {
        try
        {
            LOG_DEBUG("Processing window request kind={} on thread={}", static_cast<int>(request->kind),
                      threadIdToString(std::this_thread::get_id()));
            switch (request->kind)
            {
            case WindowRequest::Kind::Open:
                request->promise.set_value(executeOpenWindow(static_cast<OpenWindowRequest&>(*request)));
                break;
            case WindowRequest::Kind::Close:
                request->promise.set_value(executeCloseWindow(static_cast<CloseWindowRequest&>(*request)));
                break;
            case WindowRequest::Kind::CloseAll:
                request->promise.set_value(executeCloseAllWindows(static_cast<CloseAllWindowsRequest&>(*request)));
                break;
            }
        }
        catch (const std::exception& error)
        {
            request->promise.set_value(WindowResult::failure({}, "internal_error", error.what()));
        }
        catch (...)
        {
            request->promise.set_value(WindowResult::failure({}, "internal_error", "window request failed"));
        }
    }
}

void WindowService::pollEvents(int delay_ms)
{
    if (!ensureGuiThread("pollEvents"))
    {
        return;
    }

    LOG_DEBUG("Polling HighGUI events on thread={}", threadIdToString(std::this_thread::get_id()));
    backend_.pollEvents(delay_ms);
}

bool WindowService::hasOpenWindows() const
{
    return !role_to_window_id_.empty();
}

void WindowService::closeAllOnMainThread()
{
    if (!ensureGuiThread("closeAllOnMainThread"))
    {
        return;
    }

    for (const auto& [role, window_id] : role_to_window_id_)
    {
        (void)role;
        (void)backend_.closeWindow(window_id);
    }
    role_to_window_id_.clear();
    role_to_window_title_.clear();
    title_to_window_role_.clear();
    backend_.pollEvents(1);
}

bool WindowService::ensureGuiThread(const char* operation) const
{
    if (std::this_thread::get_id() == gui_thread_id_)
    {
        return true;
    }

    LOG_ERROR("HighGUI operation called from non-GUI thread: operation={}, gui_thread={}, current_thread={}", operation,
              threadIdToString(gui_thread_id_), threadIdToString(std::this_thread::get_id()));
    return false;
}

void WindowService::enqueue(std::shared_ptr<WindowRequest> request)
{
    std::lock_guard lock(queue_mutex_);
    requests_.push_back(std::move(request));
}

WindowResult WindowService::executeOpenWindow(OpenWindowRequest& request)
{
    const auto& config = request.config;
    if (role_to_window_id_.contains(config.role))
    {
        return WindowResult::failure(config.role, "window_already_open", "window role is already open: " + config.role);
    }

    const auto title = config.title.empty() ? config.role : config.title;
    if (const auto title_it = title_to_window_role_.find(title); title_it != title_to_window_role_.end())
    {
        return WindowResult::failure(config.role, "window_already_open",
                                     "window title is already used by role: " + title_it->second);
    }

    try
    {
        const auto window_id = backend_.openWindow(title, config.width, config.height, config.monitor_index,
                                                  config.fullscreen);
        if (window_id == win::kInvalidWindowId)
        {
            return WindowResult::failure(config.role, "window_open_failed", "window backend returned invalid id");
        }
        role_to_window_id_[config.role] = window_id;
        role_to_window_title_[config.role] = title;
        title_to_window_role_[title] = config.role;
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

WindowResult WindowService::executeCloseWindow(CloseWindowRequest& request)
{
    const auto it = role_to_window_id_.find(request.role);
    if (it == role_to_window_id_.end())
    {
        return WindowResult::failure(request.role, "window_not_open", "window role is not open: " + request.role);
    }

    const auto window_id = it->second;
    try
    {
        if (!backend_.closeWindow(window_id))
        {
            return WindowResult::failure(request.role, "window_close_failed",
                                         "window backend could not close id: " + std::to_string(window_id));
        }
        if (const auto title_it = role_to_window_title_.find(request.role); title_it != role_to_window_title_.end())
        {
            title_to_window_role_.erase(title_it->second);
            role_to_window_title_.erase(title_it);
        }
        role_to_window_id_.erase(it);
        return WindowResult::success(request.role, window_id, 0, 0);
    }
    catch (const std::exception& error)
    {
        return WindowResult::failure(request.role, "window_close_failed", error.what());
    }
    catch (...)
    {
        return WindowResult::failure(request.role, "internal_error", "window close failed without error detail");
    }
}

WindowResult WindowService::executeCloseAllWindows(CloseAllWindowsRequest& request)
{
    (void)request;
    closeAllOnMainThread();
    return WindowResult::success({}, win::kInvalidWindowId, 0, 0);
}

} // namespace service::window
