#include "service/projector_service.hpp"

#include "service/window_service.hpp"
#include "structured_light/structured_light.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <utility>

namespace service::projector
{

ProjectorService::ProjectorService(service::window::WindowService& window_service) : window_service_(window_service) {}

ProjectorService::~ProjectorService() = default;

ProjectorResult ProjectorService::openProjector(const ProjectorOpenConfig& config)
{
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

    ProjectorSession session;
    session.projector_role = config.projector_role;
    session.window_role = config.window_role;
    session.width = config.width;
    session.height = config.height;
    sessions_.emplace(config.projector_role, std::move(session));
    return ProjectorResult::success(config.projector_role, config.window_role, config.width, config.height, 0, -1);
}

ProjectorResult ProjectorService::closeProjector(const std::string& projector_role)
{
    auto it = sessions_.find(projector_role);
    if (it == sessions_.end())
    {
        return ProjectorResult::failure(projector_role, "projector_not_open", "projector role is not open: " + projector_role);
    }
    const auto window_role = it->second.window_role;
    const auto width = it->second.width;
    const auto height = it->second.height;
    sessions_.erase(it);
    return ProjectorResult::success(projector_role, window_role, width, height, 0, -1);
}

ProjectorResult ProjectorService::generatePatterns(const std::string& projector_role)
{
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open", "projector role is not open: " + projector_role);
    }

    try
    {
        session->structured_light = std::make_unique<sl::StructuredLight>(session->width, session->height);
        session->structured_light->generatePatterns();
        const auto count = static_cast<int>(session->structured_light->getPatternCount());
        if (count <= 0)
        {
            return ProjectorResult::failure(projector_role, "pattern_generate_failed", "no patterns generated");
        }
        session->current_index = 0;
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
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open", "projector role is not open: " + projector_role);
    }
    if (!session->structured_light || session->structured_light->getPatternCount() == 0)
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
        const auto image = session->structured_light->getPattern(static_cast<size_t>(index)).clone();
        const auto shown = window_service_.showImage(session->window_role, image);
        if (!shown.ok)
        {
            const auto message = shown.error ? shown.error->message : std::string{"failed to show pattern"};
            return ProjectorResult::failure(projector_role, "pattern_show_failed", message);
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
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open", "projector role is not open: " + projector_role);
    }
    if (!session->structured_light || session->structured_light->getPatternCount() == 0)
    {
        return ProjectorResult::failure(projector_role, "pattern_not_generated", "patterns are not generated");
    }
    const auto count = static_cast<int>(session->structured_light->getPatternCount());
    return showPattern(projector_role, (session->current_index + 1) % count);
}

ProjectorResult ProjectorService::prevPattern(const std::string& projector_role)
{
    auto* session = findSession(projector_role);
    if (!session)
    {
        return ProjectorResult::failure(projector_role, "projector_not_open", "projector role is not open: " + projector_role);
    }
    if (!session->structured_light || session->structured_light->getPatternCount() == 0)
    {
        return ProjectorResult::failure(projector_role, "pattern_not_generated", "patterns are not generated");
    }
    const auto count = static_cast<int>(session->structured_light->getPatternCount());
    return showPattern(projector_role, (session->current_index + count - 1) % count);
}

void ProjectorService::closeAll()
{
    sessions_.clear();
}

bool ProjectorService::isValidProjectorRole(const std::string& projector_role)
{
    if (projector_role.empty())
    {
        return false;
    }
    return std::all_of(projector_role.begin(), projector_role.end(), [](unsigned char ch)
                       { return std::isalnum(ch) || ch == '_' || ch == '-'; });
}

ProjectorService::ProjectorSession* ProjectorService::findSession(const std::string& projector_role)
{
    auto it = sessions_.find(projector_role);
    return it == sessions_.end() ? nullptr : &it->second;
}

ProjectorResult ProjectorService::successFromSession(const ProjectorSession& session)
{
    const auto count = session.structured_light ? static_cast<int>(session.structured_light->getPatternCount()) : 0;
    const auto index = count > 0 ? session.current_index : -1;
    return ProjectorResult::success(session.projector_role, session.window_role, session.width, session.height, count,
                                    index);
}

} // namespace service::projector
