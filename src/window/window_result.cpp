#include "window/window_result.hpp"

#include <utility>

namespace service::window
{

WindowResult WindowResult::success(std::string role, win::WindowId window_id, int width, int height)
{
    WindowResult result;
    result.ok = true;
    result.role = std::move(role);
    result.window_id = window_id;
    result.width = width;
    result.height = height;
    return result;
}

WindowResult WindowResult::failure(std::string role, std::string code, std::string message)
{
    WindowResult result;
    result.role = std::move(role);
    result.error = WindowError{std::move(code), std::move(message)};
    return result;
}

} // namespace service::window
