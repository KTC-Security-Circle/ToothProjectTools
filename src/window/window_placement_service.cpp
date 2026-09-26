#include "window/window_placement_service.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <regex>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>

#ifndef _WIN32
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

extern char** environ;

namespace win
{
namespace
{
WindowPlacementResult runProcess(const std::vector<std::string>& arguments, std::string* output = nullptr)
{
    if (arguments.empty())
        return WindowPlacementResult::failure("window_placement_failed", "empty process arguments");
    int pipe_fds[2]{-1, -1};
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (output)
    {
        if (pipe(pipe_fds) != 0)
        {
            posix_spawn_file_actions_destroy(&actions);
            return WindowPlacementResult::failure("window_placement_failed", "failed to create niri output pipe");
        }
        posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
        posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
    }
    std::vector<char*> argv;
    for (const auto& argument : arguments)
        argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    pid_t child{};
    const int spawn_error = posix_spawnp(&child, argv.front(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (output)
        close(pipe_fds[1]);
    if (spawn_error != 0)
    {
        if (output)
            close(pipe_fds[0]);
        return WindowPlacementResult::failure(
            spawn_error == ENOENT ? "window_placement_unsupported" : "window_placement_failed",
            "failed to start placement process: " + std::string(std::strerror(spawn_error)));
    }
    if (output)
    {
        char buffer[4096];
        ssize_t count{};
        while ((count = read(pipe_fds[0], buffer, sizeof(buffer))) > 0)
            output->append(buffer, static_cast<std::size_t>(count));
        close(pipe_fds[0]);
    }
    int status{};
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return WindowPlacementResult::failure("window_placement_failed", "placement process returned a failure");
    return WindowPlacementResult::success();
}

class NiriWindowPlacementBackend final : public WindowPlacementBackend
{
  public:
    WindowPlacementResult place(const WindowPlacementRequest& request) override
    {
        if (request.monitor.name.empty())
            return WindowPlacementResult::failure("window_placement_failed", "target niri output name is unavailable");
        std::string id;
        WindowPlacementResult resolved;
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            std::string json;
            auto queried = runProcess({"niri", "msg", "--json", "windows"}, &json);
            if (!queried.ok)
                return queried;
            resolved = resolveNiriWindow(parseNiriWindows(json), request.window.process_id, request.window.title, id);
            if (resolved.ok || resolved.error_code == "window_native_identity_ambiguous")
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!resolved.ok)
            return resolved;
        auto moved = runProcess({"niri", "msg", "action", "move-window-to-monitor", "--id", id, request.monitor.name});
        if (!moved.ok)
            return moved;
        if (request.fullscreen)
            return runProcess({"niri", "msg", "action", "fullscreen-window", "--id", id});
        return WindowPlacementResult::success();
    }
};

#ifndef _WIN32
std::optional<unsigned long> findX11Window(Display* display, ::Window root, int pid, const std::string& title)
{
    Atom pid_atom = XInternAtom(display, "_NET_WM_PID", True);
    std::vector<unsigned long> matches;
    std::vector<::Window> pending{root};
    while (!pending.empty())
    {
        const ::Window current = pending.back();
        pending.pop_back();
        Atom actual{};
        int format{};
        unsigned long count{}, remaining{};
        unsigned char* data{};
        int window_pid = -1;
        if (pid_atom != None &&
            XGetWindowProperty(display, current, pid_atom, 0, 1, False, XA_CARDINAL, &actual, &format, &count,
                               &remaining, &data) == Success &&
            data)
        {
            window_pid = static_cast<int>(*reinterpret_cast<unsigned long*>(data));
            XFree(data);
        }
        char* name{};
        if (window_pid == pid && XFetchName(display, current, &name) && name)
        {
            if (title == name)
                matches.push_back(current);
            XFree(name);
        }
        ::Window returned_root{}, parent{};
        ::Window* children{};
        unsigned int child_count{};
        if (XQueryTree(display, current, &returned_root, &parent, &children, &child_count))
        {
            for (unsigned int i = 0; i < child_count; ++i)
                pending.push_back(children[i]);
            if (children)
                XFree(children);
        }
    }
    return matches.size() == 1 ? std::optional<unsigned long>{matches.front()} : std::nullopt;
}

class X11WindowPlacementBackend final : public WindowPlacementBackend
{
  public:
    WindowPlacementResult place(const WindowPlacementRequest& request) override
    {
        Display* display = XOpenDisplay(nullptr);
        if (!display)
            return WindowPlacementResult::failure("window_placement_unsupported", "X11 display is unavailable");
        const auto window =
            findX11Window(display, DefaultRootWindow(display), request.window.process_id, request.window.title);
        if (!window)
        {
            XCloseDisplay(display);
            return WindowPlacementResult::failure("window_native_identity_not_found",
                                                  "X11 window was not uniquely identified");
        }
        const auto rect = x11PlacementRect(request.monitor, request.fullscreen, request.width, request.height);
        XMoveResizeWindow(display, *window, rect.x, rect.y, static_cast<unsigned int>(rect.width),
                          static_cast<unsigned int>(rect.height));
        if (request.fullscreen)
        {
            const Atom state = XInternAtom(display, "_NET_WM_STATE", False);
            const Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
            XEvent event{};
            event.xclient.type = ClientMessage;
            event.xclient.window = *window;
            event.xclient.message_type = state;
            event.xclient.format = 32;
            event.xclient.data.l[0] = 1;
            event.xclient.data.l[1] = static_cast<long>(fullscreen);
            XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask,
                       &event);
        }
        XFlush(display);
        XCloseDisplay(display);
        return WindowPlacementResult::success();
    }
};
#endif
} // namespace

WindowPlacementResult WindowPlacementResult::success()
{
    return {true, {}, {}};
}
WindowPlacementResult WindowPlacementResult::failure(std::string code, std::string message)
{
    return {false, std::move(code), std::move(message)};
}

WindowPlacementService::WindowPlacementService(WindowPlacementBackend& backend, MonitorService& monitors)
    : backend_(backend), monitors_(monitors)
{
}

WindowPlacementResult WindowPlacementService::place(const WindowIdentity& window, int monitor_index, bool fullscreen,
                                                    int width, int height)
{
    const auto monitor = monitors_.getMonitor(monitor_index);
    if (!monitor)
        return WindowPlacementResult::failure("monitor_not_found", "monitor index is unavailable");
    return backend_.place({window, *monitor, fullscreen, width, height});
}

void WindowPlacementService::forget(WindowId window_id)
{
    backend_.forget(window_id);
}

WindowEnvironment detectWindowEnvironment()
{
    WindowEnvironment result;
#ifdef _WIN32
    result.windows = true;
#else
    if (const char* value = std::getenv("XDG_SESSION_TYPE"))
        result.session_type = value;
    result.niri_socket = std::getenv("NIRI_SOCKET") != nullptr;
    result.display = std::getenv("DISPLAY") != nullptr;
#endif
    return result;
}

WindowPlacementBackendKind selectWindowPlacementBackend(const WindowEnvironment& environment)
{
    if (environment.windows)
        return WindowPlacementBackendKind::Win32;
    if (environment.session_type == "wayland" && environment.niri_socket)
        return WindowPlacementBackendKind::Niri;
    if (environment.session_type == "x11" || (environment.session_type.empty() && environment.display))
        return WindowPlacementBackendKind::X11;
    return WindowPlacementBackendKind::Unsupported;
}

std::unique_ptr<WindowPlacementBackend> createWindowPlacementBackend()
{
    switch (selectWindowPlacementBackend(detectWindowEnvironment()))
    {
    case WindowPlacementBackendKind::Niri:
        return std::make_unique<NiriWindowPlacementBackend>();
#ifndef _WIN32
    case WindowPlacementBackendKind::X11:
        return std::make_unique<X11WindowPlacementBackend>();
#endif
    default:
        return std::make_unique<UnsupportedWindowPlacementBackend>();
    }
}

std::vector<NiriWindowInfo> parseNiriWindows(const std::string& json)
{
    std::vector<NiriWindowInfo> result;
    const std::regex id_pattern(R"("id"\s*:\s*([0-9]+))");
    const std::regex title_pattern(R"regex("title"\s*:\s*"((?:\\.|[^"\\])*)")regex");
    const std::regex pid_pattern(R"("pid"\s*:\s*([0-9]+))");
    std::vector<std::string> objects;
    std::size_t start = std::string::npos;
    int depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (std::size_t index = 0; index < json.size(); ++index)
    {
        const char ch = json[index];
        if (quoted)
        {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                quoted = false;
            continue;
        }
        if (ch == '"')
        {
            quoted = true;
            continue;
        }
        if (ch == '{')
        {
            if (depth++ == 0)
                start = index;
        }
        else if (ch == '}' && depth > 0 && --depth == 0 && start != std::string::npos)
        {
            objects.push_back(json.substr(start, index - start + 1));
            start = std::string::npos;
        }
    }
    for (const auto& object : objects)
    {
        std::smatch id_match, title_match, pid_match;
        if (!std::regex_search(object, id_match, id_pattern) || !std::regex_search(object, title_match, title_pattern))
            continue;
        NiriWindowInfo info{id_match[1].str(), title_match[1].str(), std::nullopt};
        if (std::regex_search(object, pid_match, pid_pattern))
            info.pid = std::stoi(pid_match[1].str());
        result.push_back(std::move(info));
    }
    return result;
}

WindowPlacementResult resolveNiriWindow(const std::vector<NiriWindowInfo>& windows, int process_id,
                                        const std::string& title, std::string& native_id)
{
    std::vector<const NiriWindowInfo*> matches;
    for (const auto& window : windows)
        if (window.title == title && window.pid && *window.pid == process_id)
            matches.push_back(&window);
    if (matches.empty())
        return WindowPlacementResult::failure("window_native_identity_not_found",
                                              "niri window was not found by PID and title");
    if (matches.size() != 1)
        return WindowPlacementResult::failure("window_native_identity_ambiguous",
                                              "multiple niri windows matched PID and title");
    native_id = matches.front()->id;
    return WindowPlacementResult::success();
}

PlacementRect x11PlacementRect(const MonitorInfo& monitor, bool fullscreen, int width, int height)
{
    return fullscreen ? PlacementRect{monitor.x, monitor.y, monitor.width, monitor.height}
                      : PlacementRect{monitor.x, monitor.y, width, height};
}

WindowPlacementResult UnsupportedWindowPlacementBackend::place(const WindowPlacementRequest&)
{
    return WindowPlacementResult::failure("window_placement_unsupported",
                                          "window placement is unsupported in this environment");
}

} // namespace win
