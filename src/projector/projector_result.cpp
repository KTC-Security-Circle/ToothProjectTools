#include "projector/projector_result.hpp"

#include <utility>

namespace service::projector
{

ProjectorResult ProjectorResult::success(std::string projector_role, std::string window_role, int width, int height,
                                         int pattern_count, int pattern_index)
{
    ProjectorResult result;
    result.ok = true;
    result.projector_role = std::move(projector_role);
    result.window_role = std::move(window_role);
    result.width = width;
    result.height = height;
    result.code_width = width;
    result.code_height = height;
    result.pattern_count = pattern_count;
    result.pattern_index = pattern_index;
    return result;
}

ProjectorResult ProjectorResult::failure(std::string projector_role, std::string code, std::string message)
{
    ProjectorResult result;
    result.projector_role = std::move(projector_role);
    result.error = ProjectorError{std::move(code), std::move(message)};
    return result;
}

} // namespace service::projector
