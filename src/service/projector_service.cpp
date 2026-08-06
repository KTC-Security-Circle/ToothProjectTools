#include "service/projector_service.hpp"

#include "service/window_service.hpp"
#include "structured_light/structured_light.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <utility>

namespace service::projector
{
namespace
{

std::string windowErrorMessage(const service::window::WindowResult& result, const std::string& fallback)
{
    return result.error ? result.error->message : fallback;
}

} // namespace

ProjectorService::ProjectorService(service::window::WindowService& window_service,
                                   service::monitor::MonitorService& monitor_service)
    : window_service_(window_service), monitor_service_(monitor_service)
{
}

ProjectorService::~ProjectorService() = default;

ProjectorResult ProjectorService::openProjector(const ProjectorOpenConfig& config)
{
    std::lock_guard lock(mutex_);
    if (!isValidProjectorRole(config.projector_role))
    {
        return ProjectorResult::failure(config.projector_role, "invalid_projector_role",
                                        "projector_role must contain only [A-Za-z0-9_-]");
    }
    if (config.window_role.empty())
    {
        return ProjectorResult::failure(config.projector_role, "projector_window_not_open", "window_role is empty");
    }
    if (config.width <= 0 || config.height <= 0)
    {
        return ProjectorResult::failure(config.projector_role, "invalid_projector_size",
                                        "width and height must be positive");
    }
    if (sessions_.contains(config.projector_role))
    {
        return ProjectorResult::failure(config.projector_role, "projector_already_open",
                                        "projector role is already open: " + config.projector_role);
    }
    if (!window_service_.isWindowOpen(config.window_role))
    {
        return ProjectorResult::failure(config.projector_role, "projector_window_not_open",
                                        "window role is not open: " + config.window_role);
    }

    const auto resolved_monitor = monitor_service_.resolveMonitor(std::nullopt);
    if (!resolved_monitor)
    {
        return ProjectorResult::failure(config.projector_role, "monitor_not_found", "no monitors are available");
    }

    ProjectorSession session;
    session.projector_role = config.projector_role;
    session.window_role = config.window_role;
    session.code_width = config.width;
    session.code_height = config.height;
    session.surface =
        computeSurface(resolved_monitor->monitor, config.width, config.height, 0, 0, ProjectorPlacement::custom);
    session.patterns_dirty = true;
    sessions_.emplace(config.projector_role, std::move(session));
    return successFromSession(sessions_.at(config.projector_role));
}

ProjectorResult ProjectorService::listMonitors()
{
    ProjectorResult result;
    result.ok = true;
    result.monitors = monitor_service_.listMonitors();
    return result;
}

ProjectorResult ProjectorService::configureSurface(const ProjectorSurfaceRequest& request)
{
    std::lock_guard lock(mutex_);
    auto* session = findSession(request.projector_role);
    if (!session)
    {
        return ProjectorResult::failure(request.projector_role, "projector_not_open",
                                        "projector role is not open: " + request.projector_role);
    }

    const auto resolved_monitor = monitor_service_.resolveMonitor(request.monitor_index);
    if (!resolved_monitor)
    {
        return ProjectorResult::failure(request.projector_role, "monitor_not_found",
                                        "monitor not found: " + std::to_string(request.monitor_index));
    }
    const auto& monitor = resolved_monitor->monitor;
    if (monitor.width <= 0 || monitor.height <= 0)
    {
        return ProjectorResult::failure(request.projector_role, "invalid_monitor_size",
                                        "monitor width and height must be positive");
    }
    if (!window_service_.isWindowOpen(session->window_role))
    {
        return ProjectorResult::failure(request.projector_role, "projector_window_not_open",
                                        "window role is not open: " + session->window_role);
    }

    auto surface = computeSurface(monitor, request.width, request.height, request.x, request.y, request.placement);
    if (surface.pattern_width <= 0 || surface.pattern_height <= 0)
    {
        return ProjectorResult::failure(request.projector_role, "invalid_projector_surface",
                                        "effective projector surface is empty");
    }

    const auto must_configure_window =
        session->surface.monitor_index != surface.monitor_index || session->surface.monitor_x != surface.monitor_x ||
        session->surface.monitor_y != surface.monitor_y || session->surface.surface_width != surface.surface_width ||
        session->surface.surface_height != surface.surface_height;
    if (must_configure_window)
    {
        const auto window_result = window_service_.configureWindowSurface(service::window::WindowSurfaceConfig{
            session->window_role,
            surface.monitor_index,
            surface.monitor_x,
            surface.monitor_y,
            surface.surface_width,
            surface.surface_height,
            true,
        });
        if (!window_result.ok)
        {
            const auto code = window_result.error ? window_result.error->code : std::string{};
            return ProjectorResult::failure(request.projector_role,
                                            code == "window_not_open" ? "projector_window_not_open"
                                                                      : "projector_window_configure_failed",
                                            windowErrorMessage(window_result, "failed to configure projector window"));
        }
    }

    session->surface = surface;
    return successFromSession(*session);
}

ProjectorResult ProjectorService::closeProjector(const std::string& projector_role)
{
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(projector_role);
    if (it == sessions_.end())
    {
        return ProjectorResult::failure(projector_role, "projector_not_open",
                                        "projector role is not open: " + projector_role);
    }
    const auto result = successFromSession(it->second);
    sessions_.erase(it);
    return result;
}

ProjectorResult ProjectorService::generatePatterns(const std::string& projector_role)
{
    std::lock_guard lock(mutex_);
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open",
                                        "projector role is not open: " + projector_role);
    }

    if (session->code_width <= 0 || session->code_height <= 0)
    {
        return ProjectorResult::failure(projector_role, "invalid_projector_size",
                                        "code_width and code_height must be positive");
    }

    try
    {
        session->structured_light = std::make_unique<sl::StructuredLight>(session->code_width, session->code_height);
        session->structured_light->generatePatterns();
        const auto count = static_cast<int>(session->structured_light->getPatternCount());
        if (count <= 0)
        {
            return ProjectorResult::failure(projector_role, "pattern_generate_failed", "no patterns generated");
        }
        session->current_index = 0;
        session->patterns_dirty = false;
        return successFromSession(*session);
    }
    catch (const std::exception& error)
    {
        return ProjectorResult::failure(projector_role, "pattern_generate_failed", error.what());
    }
    catch (...)
    {
        return ProjectorResult::failure(projector_role, "internal_error", "pattern generation failed");
    }
}

ProjectorResult ProjectorService::showPattern(const std::string& projector_role, int index)
{
    std::lock_guard lock(mutex_);
    return showPatternLocked(projector_role, index);
}

ProjectorResult ProjectorService::showPatternLocked(const std::string& projector_role, int index)
{
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open",
                                        "projector role is not open: " + projector_role);
    }
    if (!session->structured_light || session->structured_light->getPatternCount() == 0 || session->patterns_dirty)
    {
        return ProjectorResult::failure(projector_role, "pattern_not_generated", "patterns are not generated");
    }

    const auto count = static_cast<int>(session->structured_light->getPatternCount());
    if (index < 0 || index >= count)
    {
        return ProjectorResult::failure(projector_role, "pattern_index_out_of_range", "pattern index is out of range");
    }

    try
    {
        const auto pattern = session->structured_light->getPattern(static_cast<size_t>(index)).clone();
        const auto canvas = composePatternCanvas(pattern, session->surface);
        const auto shown = window_service_.showImage(session->window_role, canvas);
        if (!shown.ok)
        {
            return ProjectorResult::failure(projector_role, "pattern_show_failed",
                                            windowErrorMessage(shown, "failed to show pattern"));
        }
        session->current_index = index;
        return successFromSession(*session);
    }
    catch (const std::exception& error)
    {
        return ProjectorResult::failure(projector_role, "pattern_show_failed", error.what());
    }
    catch (...)
    {
        return ProjectorResult::failure(projector_role, "internal_error", "pattern show failed");
    }
}

ProjectorResult ProjectorService::nextPattern(const std::string& projector_role)
{
    std::lock_guard lock(mutex_);
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open",
                                        "projector role is not open: " + projector_role);
    }
    if (!session->structured_light || session->structured_light->getPatternCount() == 0 || session->patterns_dirty)
    {
        return ProjectorResult::failure(projector_role, "pattern_not_generated", "patterns are not generated");
    }
    const auto count = static_cast<int>(session->structured_light->getPatternCount());
    return showPatternLocked(projector_role, (session->current_index + 1) % count);
}

ProjectorResult ProjectorService::prevPattern(const std::string& projector_role)
{
    std::lock_guard lock(mutex_);
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open",
                                        "projector role is not open: " + projector_role);
    }
    if (!session->structured_light || session->structured_light->getPatternCount() == 0 || session->patterns_dirty)
    {
        return ProjectorResult::failure(projector_role, "pattern_not_generated", "patterns are not generated");
    }
    const auto count = static_cast<int>(session->structured_light->getPatternCount());
    return showPatternLocked(projector_role, (session->current_index + count - 1) % count);
}

std::optional<ProjectorScanSnapshot> ProjectorService::scanSnapshot(const std::string& projector_role) const
{
    std::lock_guard lock(mutex_);
    const auto* session = findSession(projector_role);
    if (!session)
    {
        return std::nullopt;
    }

    return ProjectorScanSnapshot{
        session->projector_role,
        session->window_role,
        session->structured_light ? static_cast<int>(session->structured_light->getPatternCount()) : 0,
        session->code_width,
        session->code_height,
        session->patterns_dirty,
        session->surface,
    };
}

void ProjectorService::closeAll()
{
    std::lock_guard lock(mutex_);
    sessions_.clear();
}

bool ProjectorService::isValidProjectorRole(const std::string& projector_role)
{
    if (projector_role.empty())
    {
        return false;
    }
    return std::all_of(projector_role.begin(), projector_role.end(),
                       [](unsigned char ch) { return std::isalnum(ch) || ch == '_' || ch == '-'; });
}

ProjectorService::ProjectorSession* ProjectorService::findSession(const std::string& projector_role)
{
    auto it = sessions_.find(projector_role);
    return it == sessions_.end() ? nullptr : &it->second;
}

const ProjectorService::ProjectorSession* ProjectorService::findSession(const std::string& projector_role) const
{
    auto it = sessions_.find(projector_role);
    return it == sessions_.end() ? nullptr : &it->second;
}

ProjectorResult ProjectorService::successFromSession(const ProjectorSession& session)
{
    const auto count = session.structured_light ? static_cast<int>(session.structured_light->getPatternCount()) : 0;
    const auto index = count > 0 ? session.current_index : -1;
    auto result = ProjectorResult::success(session.projector_role, session.window_role, session.code_width,
                                           session.code_height, count, index);
    result.monitor_index = session.surface.monitor_index;
    result.monitor_x = session.surface.monitor_x;
    result.monitor_y = session.surface.monitor_y;
    result.monitor_width = session.surface.monitor_width;
    result.monitor_height = session.surface.monitor_height;
    result.surface_width = session.surface.surface_width;
    result.surface_height = session.surface.surface_height;
    result.pattern_width = session.surface.pattern_width;
    result.pattern_height = session.surface.pattern_height;
    result.display_width = session.surface.pattern_width;
    result.display_height = session.surface.pattern_height;
    result.display_x = session.surface.pattern_x;
    result.display_y = session.surface.pattern_y;
    result.pattern_x = session.surface.pattern_x;
    result.pattern_y = session.surface.pattern_y;
    result.clamped = session.surface.clamped;
    return result;
}

ProjectorSurface ProjectorService::makeDefaultSurface(int width, int height) const
{
    if (const auto resolved = monitor_service_.resolveMonitor(std::nullopt);
        resolved && resolved->monitor.width > 0 && resolved->monitor.height > 0)
    {
        return computeSurface(resolved->monitor, width, height, 0, 0, ProjectorPlacement::custom);
    }

    ProjectorSurface surface;
    surface.monitor_index = 0;
    surface.monitor_width = width;
    surface.monitor_height = height;
    surface.surface_width = width;
    surface.surface_height = height;
    surface.pattern_width = width;
    surface.pattern_height = height;
    surface.pattern_x = 0;
    surface.pattern_y = 0;
    surface.clamped = false;
    return surface;
}

ProjectorSurface ProjectorService::computeSurface(const service::monitor::MonitorInfo& monitor, int requested_width,
                                                  int requested_height, std::optional<int> requested_x,
                                                  std::optional<int> requested_y, ProjectorPlacement placement)
{
    ProjectorSurface surface;
    surface.monitor_index = monitor.monitor_index;
    surface.monitor_x = monitor.x;
    surface.monitor_y = monitor.y;
    surface.monitor_width = monitor.width;
    surface.monitor_height = monitor.height;
    surface.surface_width = monitor.width;
    surface.surface_height = monitor.height;
    surface.placement = placement;

    if (placement == ProjectorPlacement::center)
    {
        surface.pattern_width = std::min(requested_width, monitor.width);
        surface.pattern_height = std::min(requested_height, monitor.height);
        surface.pattern_x = (monitor.width - surface.pattern_width) / 2;
        surface.pattern_y = (monitor.height - surface.pattern_height) / 2;
        surface.clamped = surface.pattern_width != requested_width || surface.pattern_height != requested_height;
        return surface;
    }

    const auto raw_x = requested_x.value_or(0);
    const auto raw_y = requested_y.value_or(0);
    surface.pattern_x = std::clamp(raw_x, 0, std::max(0, monitor.width - 1));
    surface.pattern_y = std::clamp(raw_y, 0, std::max(0, monitor.height - 1));
    surface.pattern_width = std::min(requested_width, monitor.width - surface.pattern_x);
    surface.pattern_height = std::min(requested_height, monitor.height - surface.pattern_y);
    surface.clamped = surface.pattern_x != raw_x || surface.pattern_y != raw_y ||
                      surface.pattern_width != requested_width || surface.pattern_height != requested_height;
    return surface;
}

cv::Mat ProjectorService::composePatternCanvas(const cv::Mat& pattern, const ProjectorSurface& surface)
{
    if (surface.surface_width <= 0 || surface.surface_height <= 0 || surface.pattern_width <= 0 ||
        surface.pattern_height <= 0 || surface.pattern_x < 0 || surface.pattern_y < 0 ||
        surface.pattern_x + surface.pattern_width > surface.surface_width ||
        surface.pattern_y + surface.pattern_height > surface.surface_height)
    {
        throw std::runtime_error("projector pattern ROI is outside surface");
    }

    cv::Mat display_pattern;
    if (pattern.channels() == 1)
    {
        cv::cvtColor(pattern, display_pattern, cv::COLOR_GRAY2BGR);
    }
    else
    {
        display_pattern = pattern;
    }

    if (display_pattern.cols != surface.pattern_width || display_pattern.rows != surface.pattern_height)
    {
        cv::Mat resized;
        cv::resize(display_pattern, resized, cv::Size(surface.pattern_width, surface.pattern_height), 0.0, 0.0,
                   cv::INTER_NEAREST);
        display_pattern = resized;
    }

    cv::Mat canvas(surface.surface_height, surface.surface_width, display_pattern.type(), cv::Scalar::all(0));
    const cv::Rect roi{surface.pattern_x, surface.pattern_y, surface.pattern_width, surface.pattern_height};
    display_pattern.copyTo(canvas(roi));
    return canvas;
}

} // namespace service::projector
