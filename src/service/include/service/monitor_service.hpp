#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace service::monitor
{

struct MonitorInfo
{
    /// monitor_index <int>: runtime内で指定するmonitor index。
    int monitor_index{0};

    /// x <int>: desktop座標上のmonitor左上X座標。
    int x{0};

    /// y <int>: desktop座標上のmonitor左上Y座標。
    int y{0};

    /// width <int>: monitorの横幅。
    int width{0};

    /// height <int>: monitorの縦幅。
    int height{0};

    /// primary <bool>: primary monitorならtrue。
    bool primary{false};

    /// name <std::string>: 取得可能な場合のmonitor名。
    std::string name;

    /// fallback <bool>: 実monitor情報ではなくfallback値ならtrue。
    bool fallback{false};
};

struct ResolvedMonitor
{
    MonitorInfo monitor;
    bool fallback{false};
    std::size_t detected_count{0};
};

class MonitorService
{
  public:
    using MonitorProvider = std::function<std::vector<MonitorInfo>()>;

    /// @brief 実環境monitor providerを利用してMonitorServiceを構築する。
    MonitorService();

    /// @brief test用monitor providerを利用してMonitorServiceを構築する。
    explicit MonitorService(MonitorProvider provider);

    /// @brief 利用可能なmonitor一覧を返す。
    std::vector<MonitorInfo> listMonitors() const;

    /// @brief monitor_indexに対応するmonitor情報を取得する。
    std::optional<MonitorInfo> getMonitor(int monitor_index) const;

    /// @brief 0-based monitor indexを解決し、範囲外ならprimary monitorへfallbackする。
    std::optional<ResolvedMonitor> resolveMonitor(std::optional<int> requested_monitor_index) const;

  private:
    MonitorProvider provider_;
};

} // namespace service::monitor
