#include "window/monitor_service.hpp"

#include "logger/logger_macros.hpp"
#include "window/monitor.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <regex>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

extern char** environ;

namespace win
{
namespace
{

std::vector<MonitorInfo> defaultMonitorProvider()
{
    if (std::getenv("NIRI_SOCKET"))
    {
        int pipe_fds[2];
        if (pipe(pipe_fds) == 0)
        {
            posix_spawn_file_actions_t actions;
            posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
            posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
            posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
            std::vector<std::string> arguments{"niri", "msg", "--json", "outputs"};
            std::vector<char*> argv;
            for (const auto& argument : arguments)
                argv.push_back(const_cast<char*>(argument.c_str()));
            argv.push_back(nullptr);
            pid_t child{};
            const int spawn_error = posix_spawnp(&child, argv.front(), &actions, nullptr, argv.data(), environ);
            posix_spawn_file_actions_destroy(&actions);
            close(pipe_fds[1]);
            if (spawn_error == 0)
            {
                std::string json;
                char buffer[4096];
                ssize_t count{};
                while ((count = read(pipe_fds[0], buffer, sizeof(buffer))) > 0)
                    json.append(buffer, static_cast<std::size_t>(count));
                close(pipe_fds[0]);
                int status{};
                if (waitpid(child, &status, 0) >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0)
                {
                    auto monitors = parseNiriMonitorOutput(json);
                    if (!monitors.empty())
                        return monitors;
                }
            }
            else
                close(pipe_fds[0]);
        }
    }
    auto rects = win::enumerate_monitors_x11();
    std::sort(rects.begin(), rects.end(),
              [](const win::MonitorRect& left, const win::MonitorRect& right)
              {
                  if (left.x != right.x)
                  {
                      return left.x < right.x;
                  }
                  return left.y < right.y;
              });

    std::vector<MonitorInfo> monitors;
    monitors.reserve(rects.size());
    for (int index = 0; index < static_cast<int>(rects.size()); ++index)
    {
        const auto& rect = rects[static_cast<std::size_t>(index)];
        monitors.push_back(MonitorInfo{
            index,
            rect.x,
            rect.y,
            rect.width,
            rect.height,
            index == 0,
            rect.name.empty() ? "monitor-" + std::to_string(index) : rect.name,
            false,
        });
    }

    return monitors;
}

} // namespace

std::vector<MonitorInfo> parseNiriMonitorOutput(const std::string& json)
{
    const std::regex output_pattern(
        R"regex("name"\s*:\s*"([^"]+)"[\s\S]*?"logical"\s*:\s*\{\s*"x"\s*:\s*(-?[0-9]+)\s*,\s*"y"\s*:\s*(-?[0-9]+)\s*,\s*"width"\s*:\s*([0-9]+)\s*,\s*"height"\s*:\s*([0-9]+))regex");
    std::vector<MonitorInfo> monitors;
    for (auto it = std::sregex_iterator(json.begin(), json.end(), output_pattern); it != std::sregex_iterator(); ++it)
        monitors.push_back({0, std::stoi((*it)[2]), std::stoi((*it)[3]), std::stoi((*it)[4]), std::stoi((*it)[5]),
                            false, (*it)[1].str(), false});
    std::sort(monitors.begin(), monitors.end(),
              [](const MonitorInfo& left, const MonitorInfo& right)
              { return left.x != right.x ? left.x < right.x : left.y < right.y; });
    for (std::size_t index = 0; index < monitors.size(); ++index)
    {
        monitors[index].monitor_index = static_cast<int>(index);
        monitors[index].primary = index == 0;
    }
    return monitors;
}

MonitorService::MonitorService() : provider_(defaultMonitorProvider) {}

MonitorService::MonitorService(MonitorProvider provider) : provider_(std::move(provider)) {}

std::vector<MonitorInfo> MonitorService::listMonitors() const
{
    if (!provider_)
    {
        return {};
    }
    return provider_();
}

std::optional<MonitorInfo> MonitorService::getMonitor(int monitor_index) const
{
    const auto monitors = listMonitors();
    const auto it =
        std::find_if(monitors.begin(), monitors.end(),
                     [monitor_index](const MonitorInfo& monitor) { return monitor.monitor_index == monitor_index; });
    if (it == monitors.end())
    {
        return std::nullopt;
    }
    return *it;
}

std::optional<ResolvedMonitor> MonitorService::resolveMonitor(std::optional<int> requested_monitor_index) const
{
    const auto monitors = listMonitors();
    if (monitors.empty())
    {
        return std::nullopt;
    }

    const auto primary =
        std::find_if(monitors.begin(), monitors.end(), [](const MonitorInfo& monitor) { return monitor.primary; });
    const auto fallback_monitor = primary != monitors.end() ? primary : monitors.begin();

    if (!requested_monitor_index)
    {
        return ResolvedMonitor{*fallback_monitor, false, monitors.size()};
    }

    const auto requested =
        std::find_if(monitors.begin(), monitors.end(),
                     [&](const MonitorInfo& monitor) { return monitor.monitor_index == *requested_monitor_index; });
    if (requested != monitors.end())
    {
        return ResolvedMonitor{*requested, false, monitors.size()};
    }

    LOG_WARN("requested monitor index {} is unavailable; falling back to monitor {} (detected monitor count: {})",
             *requested_monitor_index, fallback_monitor->monitor_index, monitors.size());
    return ResolvedMonitor{*fallback_monitor, true, monitors.size()};
}

} // namespace win
