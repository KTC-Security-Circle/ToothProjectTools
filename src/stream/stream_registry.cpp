#include "stream/stream_registry.hpp"

#include <utility>

namespace stream {

void StreamRegistry::registerRole(
    const std::string& role,
    video::CameraId camera_id,
    std::string url) {
  std::lock_guard lock(mutex_);
  auto& entry = entries_[role];
  entry.camera_id = camera_id;
  entry.url = std::move(url);
  entry.streaming = false;
  entry.jpeg.clear();
  ++entry.sequence;
  frame_ready_.notify_all();
}

void StreamRegistry::removeRole(const std::string& role) {
  std::lock_guard lock(mutex_);
  entries_.erase(role);
  frame_ready_.notify_all();
}

void StreamRegistry::setStreaming(const std::string& role, bool streaming) {
  std::lock_guard lock(mutex_);
  auto it = entries_.find(role);
  if (it == entries_.end()) {
    return;
  }
  it->second.streaming = streaming;
  if (!streaming) {
    it->second.jpeg.clear();
  }
  ++it->second.sequence;
  frame_ready_.notify_all();
}

bool StreamRegistry::isStreaming(const std::string& role) const {
  std::lock_guard lock(mutex_);
  auto it = entries_.find(role);
  return it != entries_.end() && it->second.streaming;
}

std::optional<std::string> StreamRegistry::urlFor(const std::string& role) const {
  std::lock_guard lock(mutex_);
  auto it = entries_.find(role);
  if (it == entries_.end()) {
    return std::nullopt;
  }
  return it->second.url;
}

void StreamRegistry::publish(const std::string& role, std::vector<unsigned char> jpeg) {
  // 最新JPEGをregistryへ保存し、sequenceを進めて待機中clientを起こす。
  std::lock_guard lock(mutex_);
  auto it = entries_.find(role);
  if (it == entries_.end() || !it->second.streaming) {
    return;
  }
  it->second.jpeg = std::move(jpeg);
  ++it->second.sequence;
  frame_ready_.notify_all();
}

// after_sequenceより新しいframeが来るまで待つ。
// 戻り値はsnapshotなので、呼び出し側はmutex外で安全にsendできる。
std::optional<JpegFrame> StreamRegistry::waitForFrame(
    const std::string& role,
    std::uint64_t after_sequence,
    std::chrono::milliseconds timeout) const {
  std::unique_lock lock(mutex_);
  frame_ready_.wait_for(lock, timeout, [&] {
    auto it = entries_.find(role);
    return it == entries_.end() || !it->second.streaming ||
           (!it->second.jpeg.empty() && it->second.sequence > after_sequence);
  });

  auto it = entries_.find(role);
  if (it == entries_.end() || !it->second.streaming || it->second.jpeg.empty() ||
      it->second.sequence <= after_sequence) {
    return std::nullopt;
  }

  return JpegFrame{it->second.jpeg, it->second.sequence};
}

void StreamRegistry::wakeAll() {
  frame_ready_.notify_all();
}

} // namespace stream
