#include "stream/frame_publisher.hpp"

#include "logger/logger_macros.hpp"
#include "stream/stream_registry.hpp"
#include "video/camera.hpp"

#include <algorithm>
#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <thread>
#include <utility>
#include <vector>

namespace stream {

FramePublisher::FramePublisher(
    std::string role,
    video::Camera& camera,
    StreamRegistry& registry,
    int frames_per_second,
    int jpeg_quality)
    : role_(std::move(role)),
      camera_(camera),
      registry_(registry),
      frames_per_second_(std::max(1, frames_per_second)),
      jpeg_quality_(std::clamp(jpeg_quality, 1, 100)) {}

FramePublisher::~FramePublisher() {
  stop();
}

bool FramePublisher::start() {
  // exchange(true) は「trueに変更しつつ、変更前の値を返す」。
  // 既にtrueだった場合は多重起動済みなので、そのまま成功扱いにする。
  if (running_.exchange(true)) {
    return true;
  }

  try {
    worker_ = std::thread(&FramePublisher::run, this);
  } catch (...) {
    running_.store(false);
    return false;
  }

  return true;
}

void FramePublisher::stop() {
  // exchange(false) は「falseに変更しつつ、変更前の値を返す」。
  // 変更前がfalseなら、workerは起動していないのでjoin不要。
  if (!running_.exchange(false)) {
    return;
  }

  if (worker_.joinable()) {
    worker_.join();
  }
}

bool FramePublisher::running() const noexcept {
  return running_.load();
}

void FramePublisher::run() {
  using clock = std::chrono::steady_clock;
  const auto interval = std::chrono::milliseconds(1000 / frames_per_second_);
  auto next_frame_at = clock::now();

  while (running_.load()) {
    next_frame_at += interval;

    // Camera::getFrame() 側がmutex保護されたcloneを返すなら、
    // captureFrame() と同時に呼ばれてもCamera内部状態は壊れにくい。
    auto frame = camera_.getFrame();
    if (frame.empty()) {
      std::this_thread::sleep_until(next_frame_at);
      continue;
    }

    std::vector<unsigned char> jpeg;
    const std::vector<int> options{cv::IMWRITE_JPEG_QUALITY, jpeg_quality_};
    bool encode_ok = false;
    try {
      encode_ok = cv::imencode(".jpg", frame, jpeg, options);
    } catch (const cv::Exception& error) {
      LOG_WARN("JPEG encode OpenCV exception: role={}, error={}", role_, error.what());
    }

    if (encode_ok && !jpeg.empty()) {
      registry_.publish(role_, std::move(jpeg));
    }
    std::this_thread::sleep_until(next_frame_at);
  }
}

} // namespace stream
