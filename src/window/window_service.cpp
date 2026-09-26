#include "window/window_service.hpp"

#include "logger/logger_macros.hpp"
#include "window/monitor.hpp"
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
#include <unistd.h>
#include <utility>
#include <vector>

namespace win
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
    return std::all_of(role.begin(), role.end(),
                       [](unsigned char ch) { return std::isalnum(ch) || ch == '_' || ch == '-'; });
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
#if CV_VERSION_MAJOR > 4
    const auto backend = cv::currentUIFramework();
    return backend.empty() ? std::string{"unavailable"} : backend;
#else
    // Some OpenCV 4 builds do not expose cv::currentUIFramework() in highgui headers.
    return "unavailable";
#endif
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
        ShowImage,
        ConfigureSurface,
        CheckWindowOpen,
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
    explicit OpenWindowRequest(WindowOpenConfig open_config) : WindowRequest(Kind::Open), config(std::move(open_config))
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

struct WindowService::ShowImageRequest final : WindowRequest
{
    /// role <std::string>: 表示対象window role名。
    std::string role;

    /// image <cv::Mat>: 表示する画像。
    cv::Mat image;

    /// @brief show image requestを構築する。
    ///
    /// Args:
    ///   target_role <std::string>: 表示対象window role名。
    ///   show_image <cv::Mat>: 表示する画像。
    ///
    /// Return:
    ///   <ShowImageRequest>: show image request。
    ShowImageRequest(std::string target_role, cv::Mat show_image)
        : WindowRequest(Kind::ShowImage), role(std::move(target_role)), image(std::move(show_image))
    {
    }
};

struct WindowService::ConfigureWindowSurfaceRequest final : WindowRequest
{
    /// config <WindowSurfaceConfig>: window surface設定。
    WindowSurfaceConfig config;

    explicit ConfigureWindowSurfaceRequest(WindowSurfaceConfig surface_config)
        : WindowRequest(Kind::ConfigureSurface), config(std::move(surface_config))
    {
    }
};

struct WindowService::CheckWindowOpenRequest final : WindowRequest
{
    /// role <std::string>: 確認対象window role名。
    std::string role;

    /// @brief check window open requestを構築する。
    ///
    /// Args:
    ///   target_role <std::string>: 確認対象window role名。
    ///
    /// Return:
    ///   <CheckWindowOpenRequest>: window open確認request。
    explicit CheckWindowOpenRequest(std::string target_role)
        : WindowRequest(Kind::CheckWindowOpen), role(std::move(target_role))
    {
    }
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
    win::WindowId openWindow(const std::string& title, int width, int height) override
    {
        const auto id = windows_.createWindow(title, cv::Size{width, height}, cv::Point{0, 0});
        if (auto* window = windows_.get(id))
        {
            window->resize(win::Size{width, height});
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

    /// @brief WindowManagerでwindowへ画像を表示する。
    ///
    /// Args:
    ///   window_id <win::WindowId>: 表示対象window id。
    ///   image <const cv::Mat&>: 表示する画像。
    ///
    /// Return:
    ///   <bool>: 表示対象が存在し表示できた場合はtrue。
    bool showImage(win::WindowId window_id, const cv::Mat& image) override
    {
        auto* window = windows_.get(window_id);
        if (!window)
        {
            return false;
        }
        window->setImage(image);
        window->present();
        return true;
    }

    bool configureWindowSurface(win::WindowId window_id, int monitor_index, int x, int y, int width, int height,
                                bool fullscreen) override
    {
        auto* window = windows_.get(window_id);
        if (!window)
        {
            return false;
        }

        if (window->fullscreen())
        {
            window->setFullscreen(false);
        }
        window->setMonitorIndex(monitor_index);
        int local_x = x;
        int local_y = y;
        if (const auto rect = win::get_monitor_rect(monitor_index))
        {
            local_x = x - rect->x;
            local_y = y - rect->y;
        }
        window->move(win::Point{local_x, local_y});
        window->resize(win::Size{width, height});
        window->setFullscreen(fullscreen);
        return true;
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

class WindowService::BackendPlacementAdapter final : public WindowPlacementBackend
{
  public:
    explicit BackendPlacementAdapter(WindowBackend& backend) : backend_(backend) {}
    WindowPlacementResult place(const WindowPlacementRequest& request) override
    {
        const auto& monitor = request.monitor;
        const int width = request.fullscreen ? monitor.width : request.width;
        const int height = request.fullscreen ? monitor.height : request.height;
        if (!backend_.configureWindowSurface(request.window.internal_id, monitor.monitor_index, monitor.x, monitor.y,
                                             width, height, request.fullscreen))
            return WindowPlacementResult::failure("window_placement_failed", "window backend placement failed");
        return WindowPlacementResult::success();
    }

  private:
    WindowBackend& backend_;
};

WindowService::WindowService(win::WindowManager& windows)
    : backend_(*(owned_backend_ = std::make_unique<WindowManagerBackend>(windows))),
      monitor_service_(*(owned_monitor_service_ = std::make_unique<win::MonitorService>())),
      placement_service_(
          *(owned_placement_service_ = std::make_unique<win::WindowPlacementService>(
                *(owned_placement_backend_ = std::make_unique<BackendPlacementAdapter>(backend_)), monitor_service_))),
      gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
}

WindowService::WindowService(WindowBackend& backend)
    : backend_(backend),
      monitor_service_(
          *(owned_monitor_service_ = std::make_unique<win::MonitorService>(
                [] {
                    return std::vector<win::MonitorInfo>{{0, 0, 0, 1920, 1080, true, "test-monitor", false}};
                }))),
      placement_service_(
          *(owned_placement_service_ = std::make_unique<win::WindowPlacementService>(
                *(owned_placement_backend_ = std::make_unique<BackendPlacementAdapter>(backend_)), monitor_service_))),
      gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
}

WindowService::WindowService(win::WindowManager& windows, win::MonitorService& monitor_service)
    : backend_(*(owned_backend_ = std::make_unique<WindowManagerBackend>(windows))), monitor_service_(monitor_service),
      placement_service_(
          *(owned_placement_service_ = std::make_unique<win::WindowPlacementService>(
                *(owned_placement_backend_ = std::make_unique<BackendPlacementAdapter>(backend_)), monitor_service_))),
      gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
}

WindowService::WindowService(win::WindowManager& windows, win::MonitorService& monitor_service,
                             win::WindowPlacementService& placement_service)
    : backend_(*(owned_backend_ = std::make_unique<WindowManagerBackend>(windows))), monitor_service_(monitor_service),
      placement_service_(placement_service), gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
}

WindowService::WindowService(WindowBackend& backend, win::MonitorService& monitor_service)
    : backend_(backend), monitor_service_(monitor_service),
      placement_service_(
          *(owned_placement_service_ = std::make_unique<win::WindowPlacementService>(
                *(owned_placement_backend_ = std::make_unique<BackendPlacementAdapter>(backend_)), monitor_service_))),
      gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
}

WindowService::~WindowService() = default;

WindowService::WindowService(WindowBackend& backend, win::MonitorService& monitor_service,
                             win::WindowPlacementService& placement_service)
    : backend_(backend), monitor_service_(monitor_service), placement_service_(placement_service),
      gui_thread_id_(std::this_thread::get_id())
{
    LOG_INFO("OpenCV HighGUI backend={}", highGuiBackendName());
    LOG_INFO("WindowService GUI thread={}", threadIdToString(gui_thread_id_));
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

    if (isGuiThread())
    {
        OpenWindowRequest request{config};
        return executeOpenWindow(request);
    }

    auto request = std::make_shared<OpenWindowRequest>(config);
    auto future = request->promise.get_future();
    enqueue(request);
    return future.get();
}

WindowResult WindowService::closeWindow(const std::string& role)
{
    if (isGuiThread())
    {
        CloseWindowRequest request{role};
        return executeCloseWindow(request);
    }

    auto request = std::make_shared<CloseWindowRequest>(role);
    auto future = request->promise.get_future();
    enqueue(request);
    return future.get();
}

WindowResult WindowService::showImage(const std::string& role, const cv::Mat& image)
{
    if (isGuiThread())
    {
        ShowImageRequest request{role, image.clone()};
        return executeShowImage(request);
    }

    auto request = std::make_shared<ShowImageRequest>(role, image.clone());
    auto future = request->promise.get_future();
    enqueue(request);
    return future.get();
}

WindowResult WindowService::configureWindowSurface(const WindowSurfaceConfig& config)
{
    if (!isValidRole(config.window_role))
    {
        return WindowResult::failure(config.window_role, "invalid_window_role",
                                     "window_role must contain only [A-Za-z0-9_-]");
    }
    if (config.width <= 0 || config.height <= 0)
    {
        return WindowResult::failure(config.window_role, "invalid_window_size", "width and height must be positive");
    }
    if (config.monitor_index < 0)
    {
        return WindowResult::failure(config.window_role, "invalid_monitor_index", "monitor_index must be non-negative");
    }

    if (isGuiThread())
    {
        ConfigureWindowSurfaceRequest request{config};
        return executeConfigureWindowSurface(request);
    }

    auto request = std::make_shared<ConfigureWindowSurfaceRequest>(config);
    auto future = request->promise.get_future();
    enqueue(request);
    return future.get();
}

bool WindowService::isWindowOpen(const std::string& role)
{
    if (isGuiThread())
    {
        CheckWindowOpenRequest request{role};
        return executeCheckWindowOpen(request);
    }

    auto request = std::make_shared<CheckWindowOpenRequest>(role);
    auto future = request->promise.get_future();
    enqueue(request);
    return future.get().ok;
}

std::optional<win::WindowId> WindowService::resolveWindowId(const std::string& role) const
{
    if (!ensureGuiThread("resolveWindowId"))
    {
        return std::nullopt;
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
    if (isGuiThread())
    {
        CloseAllWindowsRequest request;
        (void)executeCloseAllWindows(request);
        return;
    }

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
            case WindowRequest::Kind::ShowImage:
                request->promise.set_value(executeShowImage(static_cast<ShowImageRequest&>(*request)));
                break;
            case WindowRequest::Kind::ConfigureSurface:
                request->promise.set_value(
                    executeConfigureWindowSurface(static_cast<ConfigureWindowSurfaceRequest&>(*request)));
                break;
            case WindowRequest::Kind::CheckWindowOpen:
                request->promise.set_value(
                    executeCheckWindowOpen(static_cast<CheckWindowOpenRequest&>(*request))
                        ? WindowResult::success(static_cast<CheckWindowOpenRequest&>(*request).role,
                                                win::kInvalidWindowId, 0, 0)
                        : WindowResult::failure(static_cast<CheckWindowOpenRequest&>(*request).role, "window_not_open",
                                                "window role is not open"));
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
    if (!ensureGuiThread("hasOpenWindows"))
    {
        return false;
    }
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
        placement_service_.forget(window_id);
    }
    role_to_window_id_.clear();
    role_to_window_title_.clear();
    title_to_window_role_.clear();
    backend_.pollEvents(1);
}

bool WindowService::ensureGuiThread(const char* operation) const
{
    if (isGuiThread())
    {
        return true;
    }

    LOG_ERROR("HighGUI operation called from non-GUI thread: operation={}, gui_thread={}, current_thread={}", operation,
              threadIdToString(gui_thread_id_), threadIdToString(std::this_thread::get_id()));
    return false;
}

bool WindowService::isGuiThread() const
{
    return std::this_thread::get_id() == gui_thread_id_;
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

    const auto resolved_monitor = monitor_service_.resolveMonitor(config.monitor_index);
    if (!resolved_monitor)
    {
        return WindowResult::failure(config.role, "monitor_not_found", "no monitors are available");
    }

    try
    {
        const auto window_id = backend_.openWindow(title, config.width, config.height);
        if (window_id == win::kInvalidWindowId)
        {
            return WindowResult::failure(config.role, "window_open_failed", "window backend returned invalid id");
        }
        const auto placement = placement_service_.place(
            WindowIdentity{window_id, config.role, title, static_cast<int>(getpid()), std::nullopt},
            resolved_monitor->monitor.monitor_index, config.fullscreen, config.width, config.height);
        if (!placement.ok)
        {
            (void)backend_.closeWindow(window_id);
            placement_service_.forget(window_id);
            return WindowResult::failure(
                config.role, placement.error_code.empty() ? "window_placement_failed" : placement.error_code,
                placement.error_message.empty() ? "window placement failed" : placement.error_message);
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
        placement_service_.forget(window_id);
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

WindowResult WindowService::executeShowImage(ShowImageRequest& request)
{
    const auto it = role_to_window_id_.find(request.role);
    if (it == role_to_window_id_.end())
    {
        return WindowResult::failure(request.role, "window_not_open", "window role is not open: " + request.role);
    }
    if (request.image.empty())
    {
        return WindowResult::failure(request.role, "invalid_window_image", "image is empty");
    }
    if (!backend_.showImage(it->second, request.image))
    {
        return WindowResult::failure(request.role, "window_show_failed", "failed to show image: " + request.role);
    }
    return WindowResult::success(request.role, it->second, request.image.cols, request.image.rows);
}

WindowResult WindowService::executeConfigureWindowSurface(ConfigureWindowSurfaceRequest& request)
{
    const auto& config = request.config;
    const auto it = role_to_window_id_.find(config.window_role);
    if (it == role_to_window_id_.end())
    {
        return WindowResult::failure(config.window_role, "window_not_open",
                                     "window role is not open: " + config.window_role);
    }

    const auto resolved_monitor = monitor_service_.resolveMonitor(config.monitor_index);
    if (!resolved_monitor)
    {
        return WindowResult::failure(config.window_role, "monitor_not_found", "no monitors are available");
    }

    if (!backend_.configureWindowSurface(it->second, resolved_monitor->monitor.monitor_index, config.x, config.y,
                                         config.width, config.height, config.fullscreen))
    {
        return WindowResult::failure(config.window_role, "window_configure_failed",
                                     "failed to configure window surface: " + config.window_role);
    }
    return WindowResult::success(config.window_role, it->second, config.width, config.height);
}

bool WindowService::executeCheckWindowOpen(CheckWindowOpenRequest& request)
{
    return role_to_window_id_.contains(request.role);
}

WindowResult WindowService::executeCloseAllWindows(CloseAllWindowsRequest& request)
{
    (void)request;
    closeAllOnMainThread();
    return WindowResult::success({}, win::kInvalidWindowId, 0, 0);
}

} // namespace win
