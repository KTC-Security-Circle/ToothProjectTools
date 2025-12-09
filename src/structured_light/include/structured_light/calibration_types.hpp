// include/structured_light/calibration_types.hpp
#pragma once

namespace sl {
  // キャリブレーションデータをまとめる構造体
  struct SystemCalibration {
    cv::Mat cam_intrinsics;     // カメラ内部行列 (3x3)
    cv::Mat cam_dist_coeffs;    // カメラ歪み係数
    cv::Mat proj_intrinsics;    // プロジェクタ内部行列 (3x3)
    cv::Mat proj_dist_coeffs;   // プロジェクタ歪み係数
    cv::Mat rotation;           // 回転行列 R (3x3)
    cv::Mat translation;        // 並進ベクトル T (3x1)
    cv::Size cam_size;          // カメラ解像度
    cv::Size proj_size;         // プロジェクタ解像度
    bool ready{false};          // データが有効か
  };
}
