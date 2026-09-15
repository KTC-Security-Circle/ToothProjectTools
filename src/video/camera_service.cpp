#include "video/camera_service.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace service::camera
{

CameraResult CameraResult::success(video::CameraId camera_id, std::string role)
{
    return CameraResult{true, camera_id, std::move(role), std::nullopt};
}

CameraResult CameraResult::failure(video::CameraId camera_id, std::string role, std::string code, std::string message)
{
    return CameraResult{false, camera_id, std::move(role), CameraError{std::move(code), std::move(message)}};
}

CameraService::CameraService(video::CameraManager& cameras) : cameras_(cameras) {}

CameraResult CameraService::openCamera(video::CameraId camera_id, const std::string& role)
{
    std::lock_guard lock(mutex_);
    if (camera_id < 0)
        return CameraResult::failure(camera_id, role, "invalid_command", "camera_id must be non-negative");
    if (!validRole(role))
        return CameraResult::failure(camera_id, role, "invalid_command", "invalid camera role: " + role);
    if (const auto existing = role_to_camera_id_.find(role); existing != role_to_camera_id_.end())
    {
        cameras_.remove(existing->second);
        role_to_camera_id_.erase(existing);
        role_to_device_index_.erase(role);
    }
    video::CameraOptions options;
    options.device_index = camera_id;
    const auto managed_id = cameras_.createCamera(options, "Sidecar-" + role);
    if (managed_id == video::kInvalidCameraId)
    {
        std::ostringstream message;
        message << "failed to open camera " << camera_id;
        return CameraResult::failure(camera_id, role, "camera_open_failed", message.str());
    }
    role_to_camera_id_[role] = managed_id;
    role_to_device_index_[role] = camera_id;
    return CameraResult::success(camera_id, role);
}

CameraResult CameraService::closeCamera(const std::string& role)
{
    std::lock_guard lock(mutex_);
    const auto camera_it = role_to_camera_id_.find(role);
    if (camera_it == role_to_camera_id_.end())
        return CameraResult::failure(video::kInvalidCameraId, role, "camera_not_open", "camera_not_open");
    const auto device_it = role_to_device_index_.find(role);
    const auto device_index = device_it != role_to_device_index_.end() ? device_it->second : video::kInvalidCameraId;
    cameras_.remove(camera_it->second);
    role_to_camera_id_.erase(camera_it);
    role_to_device_index_.erase(role);
    return CameraResult::success(device_index, role);
}

std::optional<video::CameraId> CameraService::resolveCameraId(const std::string& role) const
{
    std::lock_guard lock(mutex_);
    const auto it = role_to_camera_id_.find(role);
    return it == role_to_camera_id_.end() ? std::nullopt : std::optional<video::CameraId>{it->second};
}

std::optional<video::FrameSample> CameraService::latestFrame(video::CameraId camera_id) const
{
    auto* camera = cameras_.get(camera_id);
    return camera ? camera->getFrameSample() : std::nullopt;
}

std::optional<video::FrameSample> CameraService::firstFrameAtOrAfter(
    video::CameraId camera_id, std::chrono::steady_clock::time_point timestamp) const
{
    auto* camera = cameras_.get(camera_id);
    return camera ? camera->firstFrameAtOrAfter(timestamp) : std::nullopt;
}

bool CameraService::validRole(const std::string& role)
{
    return !role.empty() &&
           std::all_of(role.begin(), role.end(),
                       [](unsigned char c) { return std::isalnum(c) || c == (char)95 || c == (char)45; });
}

} // namespace service::camera
