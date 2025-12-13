#pragma once
#include <cstdint>
#include <opencv2/videoio.hpp> // cv::VideoWriter::fourcc

namespace video {

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
