#pragma once
#include <vector>
#include <string>
#include <opencv2/core.hpp>
#include "calibration/stereo_data.hpp"
#include "calibration/calibrator.hpp" // BoardConfigのため

namespace calib {

class StereoCalibrator {
public:
    StereoCalibrator();
    ~StereoCalibrator() = default;

    // ボード設定 (Calibratorと同じ設定を使うこと)
    void setBoardConfig(const BoardConfig& config);

    /**
     * @brief ステレオキャリブレーションを実行
     * * @param files_L 左カメラの画像パスリスト
     * @param files_R 右カメラの画像パスリスト (Lと同じ順序・枚数であること)
     * @param K1 左カメラの内部パラメータ (Camera Matrix)
     * @param D1 左カメラの歪み係数 (Dist Coefficients)
     * @param K2 右カメラの内部パラメータ
     * @param D2 右カメラの歪み係数
     * @param out_data 計算結果の格納先
     * @return double RMS再投影誤差
     */
    double run(
        const std::vector<std::string>& files_L,
        const std::vector<std::string>& files_R,
        const cv::Mat& K1, const cv::Mat& D1,
        const cv::Mat& K2, const cv::Mat& D2,
        StereoData& out_data
    );

private:
    BoardConfig config_;
};

} // namespace calib
