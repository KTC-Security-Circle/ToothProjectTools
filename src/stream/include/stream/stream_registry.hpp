#pragma once

#include "video/video_types.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace stream {

struct JpegFrame {
  std::vector<unsigned char> bytes;
  std::uint64_t sequence{0};
};

// FramePublisher と MjpegServer の同期点。
// roleごとのstream状態と最新JPEG frameを保持する。
class StreamRegistry {
public:
  StreamRegistry() = default;
  ~StreamRegistry() = default;

  // mutex / condition_variableを持つ共有状態なのでcopy/move禁止。
  StreamRegistry(const StreamRegistry&) = delete;
  StreamRegistry& operator=(const StreamRegistry&) = delete;
  StreamRegistry(StreamRegistry&&) = delete;
  StreamRegistry& operator=(StreamRegistry&&) = delete;

  void registerRole(const std::string& role, video::CameraId camera_id, std::string url);
  void removeRole(const std::string& role);

  void setStreaming(const std::string& role, bool streaming);
  bool isStreaming(const std::string& role) const;
  std::optional<std::string> urlFor(const std::string& role) const;

  void publish(const std::string& role, std::vector<unsigned char> jpeg);
  std::optional<JpegFrame> waitForFrame(
      const std::string& role,
      std::uint64_t after_sequence,
      std::chrono::milliseconds timeout) const;

  void wakeAll();

private:
  struct Entry {
    video::CameraId camera_id{video::kInvalidCameraId};
    std::string url;
    bool streaming{false};
    std::vector<unsigned char> jpeg;
    std::uint64_t sequence{0};
  };

  mutable std::mutex mutex_;
  mutable std::condition_variable frame_ready_;
  std::map<std::string, Entry> entries_;
};

} // namespace stream
