#pragma once
#include <cstdint>
#include <chrono>
#include <deque>
#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp> // cv::VideoWriter::fourcc

namespace video {

  /**
   * @brief Camera frameとhost monotonic clock上の取得時刻を束ねる。
   *
   * timestampはOpenCVまたはV4L2のhardware timestampではなく、
   * VideoCapture::read()が成功した直後にhostで取得した時刻である。
   * sequenceはCameraごとに単調増加するframe番号である。
   */
  struct FrameSample {
    cv::Mat image;
    std::uint64_t sequence{0};
    std::chrono::steady_clock::time_point timestamp{};
  };

  using FrameRingBuffer = std::deque<FrameSample>;

  using CameraId = std::uint32_t;
  constexpr CameraId kInvalidCameraId = 0;

  struct CameraOptions {
    int    device_index{0};       // /dev/videoX
    int    width{1280};
    int    height{720};
    double fps{30.0};
    int    fourcc{cv::VideoWriter::fourcc('M', 'J', 'P', 'G')};
    bool   auto_exposure{true};
    // 必要に応じて露光時間やゲイン設定を追加
  };

} // namespace video
