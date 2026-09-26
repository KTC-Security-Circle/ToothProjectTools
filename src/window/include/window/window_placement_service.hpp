#pragma once

#include "window/monitor_service.hpp"
#include "window/window_types.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace win
{

struct WindowIdentity
{
    WindowId internal_id{kInvalidWindowId};
    std::string role;
    std::string title;
    int process_id{0};
    std::optional<std::string> native_id;
};

struct WindowPlacementRequest
{
    WindowIdentity window;
    MonitorInfo monitor;
    bool fullscreen{false};
    int width{0};
    int height{0};
};

struct WindowPlacementResult
{
    bool ok{false};
    std::string error_code;
    std::string error_message;

    static WindowPlacementResult success();
    static WindowPlacementResult failure(std::string code, std::string message);
};

class WindowPlacementBackend
{
  public:
    virtual ~WindowPlacementBackend() = default;
    virtual WindowPlacementResult place(const WindowPlacementRequest& request) = 0;
    virtual void forget(WindowId) {}
};

class WindowPlacementService
{
  public:
    WindowPlacementService(WindowPlacementBackend& backend, MonitorService& monitors);
    WindowPlacementResult place(const WindowIdentity& window, int monitor_index, bool fullscreen, int width,
                                int height);
    void forget(WindowId window_id);

  private:
    WindowPlacementBackend& backend_;
    MonitorService& monitors_;
};

enum class WindowPlacementBackendKind
{
    Niri,
    X11,
    Win32,
    Unsupported
};

struct WindowEnvironment
{
    bool windows{false};
    std::string session_type;
    bool niri_socket{false};
    bool display{false};
};

WindowEnvironment detectWindowEnvironment();
WindowPlacementBackendKind selectWindowPlacementBackend(const WindowEnvironment& environment);
std::unique_ptr<WindowPlacementBackend> createWindowPlacementBackend();

struct NiriWindowInfo
{
    std::string id;
    std::string title;
    std::optional<int> pid;
};

WindowPlacementResult resolveNiriWindow(const std::vector<NiriWindowInfo>& windows, int process_id,
                                        const std::string& title, std::string& native_id);
std::vector<NiriWindowInfo> parseNiriWindows(const std::string& json);

struct PlacementRect
{
    int x;
    int y;
    int width;
    int height;
};
PlacementRect x11PlacementRect(const MonitorInfo& monitor, bool fullscreen, int width, int height);

class UnsupportedWindowPlacementBackend final : public WindowPlacementBackend
{
  public:
    WindowPlacementResult place(const WindowPlacementRequest& request) override;
};

} // namespace win
