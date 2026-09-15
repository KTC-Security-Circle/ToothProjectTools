#include "command_result_mapper/camera_command_result_mapper.hpp"

#include <map>
#include <string>
#include <utility>

namespace command_result_mapper::camera
{

common::CommandResult toCommandResult(const video::CameraResult& result, bool include_camera_id)
{
    if (!result.ok)
    {
        if (result.error)
        {
            return common::failure(result.error->code, result.error->message);
        }
        return common::failure("internal_error", "camera command failed without error detail");
    }

    auto values = std::map<std::string, std::string>{{"role", result.role}};
    if (include_camera_id)
    {
        values.emplace("camera_id", std::to_string(result.camera_id));
    }
    return common::success(std::move(values));
}

} // namespace command_result_mapper::camera
