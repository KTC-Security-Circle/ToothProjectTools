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

namespace service {

SidecarResult SidecarResult::success(std::string value) {
  return SidecarResult{true, std::nullopt, std::move(value)};
}

SidecarResult SidecarResult::failure(SidecarErrorCode code, std::string message) {
  return SidecarResult{false, SidecarError{{code, message}}, {}};
}

SidecarService::SidecarService(
    std::string mjpeg_host,
    int mjpeg_port,
    stream::StreamRegistry& streams)
    : mjpeg_host_(std::move(mjpeg_host)),
      mjpeg_port_(mjpeg_port),
      streams_(streams) {}

SidecarService::~SidecarService() {
  shutdown();
}

SidecarResult SidecarService::openCamera(int device_index, const std::string& role) {
  if (device_index < 0) {
    return SidecarResult::failure(SidecarErrorCode::CameraOpenFailed, "camera_id must be non-negative");
  }
  if (!validRole(role)) {
    return SidecarResult::failure(SidecarErrorCode::InvalidCommand, "role contains unsupported characters");
  }

  if (bindings_.contains(role)) {
    closeCamera(role);
  }

  video::CameraOptions options;
  options.device_index = device_index;
  const auto camera_id = cameras_.createCamera(options, "Sidecar-" + role);
  if (camera_id == video::kInvalidCameraId) {
    std::ostringstream message;
    message << "failed to open camera " << device_index;
    return SidecarResult::failure(SidecarErrorCode::CameraOpenFailed, message.str());
  }

  CameraBinding binding;
  binding.device_index = device_index;
  binding.camera_id = camera_id;
  bindings_.emplace(role, std::move(binding));
  streams_.registerRole(role, camera_id, streamUrl(role));
  return SidecarResult::success();
}

SidecarResult SidecarService::closeCamera(const std::string& role) {
  auto it = bindings_.find(role);
  if (it == bindings_.end()) {
    return SidecarResult::failure(SidecarErrorCode::CameraNotOpen, "camera role is not open: " + role);
  }

  if (it->second.publisher) {
    it->second.publisher->stop();
    it->second.publisher.reset();
  }
  streams_.setStreaming(role, false);
  streams_.removeRole(role);
  cameras_.remove(it->second.camera_id);
  bindings_.erase(it);
  return SidecarResult::success();
}

SidecarResult SidecarService::startStream(const std::string& role) {
  auto it = bindings_.find(role);
  if (it == bindings_.end()) {
    return SidecarResult::failure(SidecarErrorCode::CameraNotOpen, "camera role is not open: " + role);
  }

  if (it->second.publisher && it->second.publisher->running()) {
    return SidecarResult::success(streamUrl(role));
  }

  auto* camera = cameras_.get(it->second.camera_id);
  if (!camera || !camera->isOpened()) {
    return SidecarResult::failure(SidecarErrorCode::CameraNotOpen, "camera role is not open: " + role);
  }

  auto publisher = std::make_unique<stream::FramePublisher>(role, *camera, streams_);
  streams_.setStreaming(role, true);
  if (!publisher->start()) {
    streams_.setStreaming(role, false);
    return SidecarResult::failure(SidecarErrorCode::StreamStartFailed, "failed to start stream: " + role);
  }
  it->second.publisher = std::move(publisher);
  return SidecarResult::success(streamUrl(role));
}

SidecarResult SidecarService::stopStream(const std::string& role) {
  auto it = bindings_.find(role);
  if (it == bindings_.end()) {
    return SidecarResult::failure(SidecarErrorCode::CameraNotOpen, "camera role is not open: " + role);
  }

  if (it->second.publisher) {
    it->second.publisher->stop();
    it->second.publisher.reset();
  }
  streams_.setStreaming(role, false);
  return SidecarResult::success();
}

SidecarResult SidecarService::captureFrame(
    const std::string& role,
    const std::string& output) {
  auto it = bindings_.find(role);
  if (it == bindings_.end()) {
    return SidecarResult::failure(SidecarErrorCode::CameraNotOpen, "camera role is not open: " + role);
  }

  auto* camera = cameras_.get(it->second.camera_id);
  if (!camera || !camera->isOpened()) {
    return SidecarResult::failure(SidecarErrorCode::CameraNotOpen, "camera role is not open: " + role);
  }

  auto frame = camera->getFrame();
  if (frame.empty()) {
    return SidecarResult::failure(SidecarErrorCode::CaptureFailed, "camera has no frame yet: " + role);
  }

  try {
    const std::filesystem::path output_path{output};
    if (output_path.has_parent_path()) {
      std::filesystem::create_directories(output_path.parent_path());
    }
    if (!cv::imwrite(output, frame)) {
      return SidecarResult::failure(SidecarErrorCode::FileWriteFailed, "failed to write image: " + output);
    }
  } catch (const std::exception& error) {
    LOG_ERROR("Frame save failed for '{}': {}", output, error.what());
    return SidecarResult::failure(SidecarErrorCode::FileWriteFailed, "failed to write image: " + output);
  }

  return SidecarResult::success(output);
}

void SidecarService::shutdown() {
  for (auto& [role, binding] : bindings_) {
    if (binding.publisher) {
      binding.publisher->stop();
      binding.publisher.reset();
    }
    streams_.setStreaming(role, false);
    streams_.removeRole(role);
  }
  bindings_.clear();
  cameras_.clear();
}

bool SidecarService::validRole(const std::string& role) const {
  return !role.empty() &&
         std::all_of(role.begin(), role.end(), [](unsigned char character) {
           return std::isalnum(character) || character == '_' || character == '-';
         });
}

std::string SidecarService::streamUrl(const std::string& role) const {
  std::ostringstream url;
  url << "http://" << mjpeg_host_ << ':' << mjpeg_port_ << '/' << role << ".mjpg";
  return url.str();
}

} // namespace service
