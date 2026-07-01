#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace video {
class Camera;
}

namespace stream {

class StreamRegistry;

// Cameraからframeを取得し、JPEGへencodeしてStreamRegistryへpublishする worker。
// Camera と StreamRegistry は所有せず、SidecarService / ServeApp 側の生存期間に依存する。
class FramePublisher {
public:
  FramePublisher(
      std::string role,
      video::Camera& camera,
      StreamRegistry& registry,
      int frames_per_second = 10,
      int jpeg_quality = 80);
  ~FramePublisher();

  // worker thread と参照メンバを持つため、copy/moveともに禁止する。
  FramePublisher(const FramePublisher&) = delete;
  FramePublisher& operator=(const FramePublisher&) = delete;
  FramePublisher(FramePublisher&&) = delete;
  FramePublisher& operator=(FramePublisher&&) = delete;

  bool start();
  void stop();
  bool running() const noexcept;

private:
  void run();

  std::string role_;
  video::Camera& camera_;
  StreamRegistry& registry_;
  int frames_per_second_;
  int jpeg_quality_;

  // 複数threadから読み書きされる停止フラグ。
  // load/exchangeで読み書きすることでdata raceを避ける。
  std::atomic<bool> running_{false};

  std::thread worker_;
};

} // namespace stream
