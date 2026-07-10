#include "service/sidecar_service.hpp"

#include "logger/logger_macros.hpp"
#include "stream/frame_publisher.hpp"
#include "stream/stream_registry.hpp"
#include "video/camera.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <utility>

namespace service
{

SidecarResult SidecarResult::success(std::string value)
{
    return SidecarResult{true, std::nullopt, std::move(value)};
}

SidecarResult SidecarResult::failure(SidecarErrorCode code, std::string message)
{
    return SidecarResult{false, SidecarError{code, message}, {}};
}

SidecarService::SidecarService(std::string mjpeg_host, int mjpeg_port, stream::StreamRegistry& streams)
    : mjpeg_host_(std::move(mjpeg_host)), mjpeg_port_(mjpeg_port), streams_(streams)
{
}

SidecarService::~SidecarService()
{
    shutdown();
}

std::string_view toString(SidecarErrorCode code)
{
    using enum SidecarErrorCode;

    switch (code)
    {
    case InvalidJson:
        return "invalid_json";
    case InvalidCommand:
        return "invalid_command";
    case MissingField:
        return "missing_field";
    case InvalidRole:
        return "invalid_role";
    case CameraOpenFailed:
        return "camera_open_failed";
    case CameraNotOpen:
        return "camera_not_open";
    case StreamStartFailed:
        return "stream_start_failed";
    case CaptureFailed:
        return "capture_failed";
    case FileWriteFailed:
        return "file_write_failed";
    case InternalError:
        return "internal_error";
    }

    return "internal_error";
}

camera::CameraResult SidecarService::openCamera(video::CameraId device_index, const std::string& role)
{
    return camera_service_.openCamera(device_index, role);
}

camera::CameraResult SidecarService::closeCamera(const std::string& role)
{
    stopStreamIfRunning(role);
    streams_.removeRole(role);
    bindings_.erase(role);
    return camera_service_.closeCamera(role);
}

SidecarResult SidecarService::startStream(const std::string& role)
{
    const auto camera_id = camera_service_.resolveCameraId(role);
    if (!camera_id)
    {
        return SidecarResult::failure(SidecarErrorCode::CameraNotOpen,
                                      toString(SidecarErrorCode::CameraNotOpen).data());
    }
    auto& binding = bindings_[role];

    if (binding.publisher && binding.publisher->running())
    {
        return SidecarResult::success(streamUrl(role));
    }

    auto* camera = cameras_.get(*camera_id);
    if (!camera || !camera->isOpened())
    {
        return SidecarResult::failure(SidecarErrorCode::CameraNotOpen,
                                      toString(SidecarErrorCode::CameraNotOpen).data());
    }

    streams_.registerRole(role, *camera_id, streamUrl(role));
    auto publisher = std::make_unique<stream::FramePublisher>(role, *camera, streams_);
    streams_.setStreaming(role, true);
    if (!publisher->start())
    {
        streams_.setStreaming(role, false);
        return SidecarResult::failure(SidecarErrorCode::StreamStartFailed,
                                      toString(SidecarErrorCode::StreamStartFailed).data());
    }
    binding.publisher = std::move(publisher);
    return SidecarResult::success(streamUrl(role));
}

SidecarResult SidecarService::stopStream(const std::string& role)
{
    stopStreamIfRunning(role);
    return camera_service_.resolveCameraId(role)
               ? SidecarResult::success()
               : SidecarResult::failure(SidecarErrorCode::CameraNotOpen,
                                        toString(SidecarErrorCode::CameraNotOpen).data());
}

void SidecarService::stopStreamIfRunning(const std::string& role)
{
    auto it = bindings_.find(role);
    if (it != bindings_.end() && it->second.publisher)
    {
        it->second.publisher->stop();
        it->second.publisher.reset();
    }
    streams_.setStreaming(role, false);
    streams_.removeRole(role);
}

SidecarResult SidecarService::captureFrame(const std::string& role, const std::string& output)
{
    const auto camera_id = camera_service_.resolveCameraId(role);
    if (!camera_id)
    {
        return SidecarResult::failure(SidecarErrorCode::CameraNotOpen,
                                      toString(SidecarErrorCode::CameraNotOpen).data());
    }

    auto* camera = cameras_.get(*camera_id);
    if (!camera || !camera->isOpened())
    {
        return SidecarResult::failure(SidecarErrorCode::CameraNotOpen,
                                      toString(SidecarErrorCode::CameraNotOpen).data());
    }

    auto frame = camera->getFrame();
    if (frame.empty())
    {
        return SidecarResult::failure(SidecarErrorCode::CaptureFailed,
                                      toString(SidecarErrorCode::CaptureFailed).data());
    }

    try
    {
        const std::filesystem::path output_path{output};
        if (output_path.has_parent_path())
        {
            std::filesystem::create_directories(output_path.parent_path());
        }
        if (!cv::imwrite(output, frame))
        {
            return SidecarResult::failure(SidecarErrorCode::FileWriteFailed,
                                          toString(SidecarErrorCode::FileWriteFailed).data());
        }
    }
    catch (const std::exception& error)
    {
        LOG_ERROR("Frame save failed for '{}': {}", output, error.what());
        return SidecarResult::failure(SidecarErrorCode::FileWriteFailed,
                                      toString(SidecarErrorCode::FileWriteFailed).data());
    }

    return SidecarResult::success(output);
}

std::optional<video::CameraId> SidecarService::resolveCameraId(const std::string& role) const
{
    return camera_service_.resolveCameraId(role);
}

camera::CameraService& SidecarService::cameraService()
{
    return camera_service_;
}

window::WindowService& SidecarService::windowService()
{
    return window_service_;
}

projector::ProjectorService& SidecarService::projectorService()
{
    return projector_service_;
}

capture::CaptureService& SidecarService::captureService()
{
    return capture_service_;
}

video::CameraManager& SidecarService::cameraManager()
{
    return cameras_;
}

calib::Calibrator* SidecarService::calibrator()
{
    return &calibrator_;
}

calib::StereoCalibrator* SidecarService::stereoCalibrator()
{
    return &stereo_calibrator_;
}

calib::StereoData& SidecarService::stereoData()
{
    return stereo_data_;
}

void SidecarService::shutdown()
{
    for (auto& [role, binding] : bindings_)
    {
        if (binding.publisher)
        {
            binding.publisher->stop();
            binding.publisher.reset();
        }
        streams_.setStreaming(role, false);
        streams_.removeRole(role);
    }
    bindings_.clear();
    projector_service_.closeAll();
    cameras_.clear();
}

std::string SidecarService::streamUrl(const std::string& role) const
{
    std::ostringstream url;
    url << "http://" << mjpeg_host_ << ':' << mjpeg_port_ << '/' << role << ".mjpg";
    return url.str();
}

} // namespace service
