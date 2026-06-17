#pragma once

#include "video/camera_manager.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <optional>

namespace stream
{
class FramePublisher;
class StreamRegistry;
} // namespace stream

namespace service
{

enum class SidecarErrorCode
{
    InvalidJson,
    InvalidCommand,
    MissingField,
    InvalidRole,
    CameraOpenFailed,
    CameraNotOpen,
    StreamStartFailed,
    CaptureFailed,
    FileWriteFailed,
    InternalError,
};

std::string_view toString(SidecarErrorCode code);

struct SidecarError
{
    SidecarErrorCode code;
    std::string message;
};

struct SidecarResult
{
    bool ok{false};
    std::optional<SidecarError> error;
    std::string value;

    static SidecarResult success(std::string value = {});
    static SidecarResult failure(SidecarErrorCode code, std::string message);
};

class SidecarService
{
  public:
    SidecarService(std::string mjpeg_host, int mjpeg_port, stream::StreamRegistry& streams);
    ~SidecarService();

    SidecarService(const SidecarService&) = delete;
    SidecarService& operator=(const SidecarService&) = delete;
    SidecarService(SidecarService&&) = delete;
    SidecarService& operator=(SidecarService&&) = delete;

    SidecarResult openCamera(int device_index, const std::string& role);
    SidecarResult closeCamera(const std::string& role);
    SidecarResult startStream(const std::string& role);
    SidecarResult stopStream(const std::string& role);
    SidecarResult captureFrame(const std::string& role, const std::string& output);
    void shutdown();

  private:
    struct CameraBinding
    {
        int device_index{-1};
        video::CameraId camera_id{video::kInvalidCameraId};
        std::unique_ptr<stream::FramePublisher> publisher;
    };

    bool validRole(const std::string& role) const;
    std::string streamUrl(const std::string& role) const;

    std::string mjpeg_host_;
    int mjpeg_port_;
    stream::StreamRegistry& streams_;
    video::CameraManager cameras_;
    std::map<std::string, CameraBinding> bindings_;
};

} // namespace service
