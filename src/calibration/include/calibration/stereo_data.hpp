#pragma once
#include <opencv2/core.hpp>

namespace calib {

// ステレオキャリブレーションの結果
struct StereoData {
    bool valid = false;
    double rms = 0.0;

    // 外部パラメータ (左カメラ基準)
    cv::Mat R; // 回転行列 (3x3)
    cv::Mat T; // 並進ベクトル (3x1)

    // 平行化 (Rectification) 用パラメータ
    cv::Mat R1, R2; // 左右それぞれの回転行列
    cv::Mat P1, P2; // 平行化後の投影行列
    cv::Mat Q;      // 視差-深度変換行列 (4x4)

    // リマップ用ルックアップテーブル (initUndistortRectifyMapの結果)
    // これを remap 関数に渡すと高速に平行化画像が作れる
    cv::Mat mapL_x, mapL_y;
    cv::Mat mapR_x, mapR_y;
};

} // namespace calib
