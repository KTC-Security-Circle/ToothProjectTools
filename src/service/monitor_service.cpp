#include "service/monitor_service.hpp"

#include "logger/logger_macros.hpp"
#include "window/monitor.hpp"

#include <algorithm>
#include <utility>

namespace service::monitor
{
namespace
{

std::vector<MonitorInfo> defaultMonitorProvider()
{
    auto rects = win::enumerate_monitors_x11();
    std::sort(rects.begin(), rects.end(), [](const win::MonitorRect& left, const win::MonitorRect& right)
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
            "monitor-" + std::to_string(index),
            false,
        });
    }

    if (!monitors.empty())
    {
        return monitors;
    }

    LOG_DEBUG("MonitorService fallback monitor used: 1920x1080 default");
    return {MonitorInfo{0, 0, 0, 1920, 1080, true, "default", true}};
}

} // namespace

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
    const auto it = std::find_if(monitors.begin(), monitors.end(), [monitor_index](const MonitorInfo& monitor)
                                 { return monitor.monitor_index == monitor_index; });
    if (it == monitors.end())
    {
        return std::nullopt;
    }
    return *it;
}

} // namespace service::monitor
