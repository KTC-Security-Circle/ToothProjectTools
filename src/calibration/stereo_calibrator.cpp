#include "calibration/stereo_calibrator.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace calib {

StereoCalibrator::StereoCalibrator() = default;

void StereoCalibrator::setBoardConfig(const BoardConfig& config) {
    config_ = config;
}

double StereoCalibrator::run(
    const std::vector<std::string>& files_L,
    const std::vector<std::string>& files_R,
    const cv::Mat& K1, const cv::Mat& D1,
    const cv::Mat& K2, const cv::Mat& D2,
    StereoData& out_data
) {
    // 1. 基本チェック
    if (files_L.size() != files_R.size()) {
        LOG_ERROR("Stereo: 画像枚数不一致 (L:{} != R:{})", files_L.size(), files_R.size());
        return -1.0;
    }
    if (files_L.empty()) return -1.0;

    LOG_INFO("Stereo: キャリブレーション開始 ({}ペア)", files_L.size());

    // 2. 3D点群の定義 (Z=0)
    std::vector<cv::Point3f> objp;
    for (int i = 0; i < config_.pattern_size.height; i++) {
        for (int j = 0; j < config_.pattern_size.width; j++) {
            objp.emplace_back(j * config_.square_size_mm, i * config_.square_size_mm, 0.0f);
        }
    }

    // 3. 画像読み込み & コーナー検出
    std::vector<std::vector<cv::Point3f>> object_points;
    std::vector<std::vector<cv::Point2f>> image_points_L;
    std::vector<std::vector<cv::Point2f>> image_points_R;
    cv::Size img_size;

    int valid_pairs = 0;
    int detect_flags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE;

    for (size_t i = 0; i < files_L.size(); ++i) {
        cv::Mat imgL = cv::imread(files_L[i], cv::IMREAD_GRAYSCALE);
        cv::Mat imgR = cv::imread(files_R[i], cv::IMREAD_GRAYSCALE);

        if (imgL.empty() || imgR.empty()) continue;
        if (img_size.area() == 0) img_size = imgL.size();
        if (imgL.size() != img_size || imgR.size() != img_size) continue;

        std::vector<cv::Point2f> cornersL, cornersR;
        
        bool foundL = cv::findChessboardCorners(imgL, config_.pattern_size, cornersL, detect_flags);
        bool foundR = cv::findChessboardCorners(imgR, config_.pattern_size, cornersR, detect_flags);

        if (foundL && foundR) {
            // サブピクセル精度向上
            cv::cornerSubPix(imgL, cornersL, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01));
            cv::cornerSubPix(imgR, cornersR, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01));

            image_points_L.push_back(cornersL);
            image_points_R.push_back(cornersR);
            object_points.push_back(objp);
            valid_pairs++;
        }
        
        if ((i+1) % 5 == 0) {
            LOG_INFO("Stereo: 検出進捗 {}/{}", i+1, files_L.size());
        }
    }

    if (valid_pairs < 5) {
        LOG_ERROR("Stereo: 有効なペアが少なすぎます ({}/{})", valid_pairs, files_L.size());
        return -1.0;
    }
    LOG_INFO("Stereo: 有効ペア {}組。計算開始...", valid_pairs);

    // 4. ステレオキャリブレーション (Extrinsicsの計算)
    // 重要: CALIB_FIX_INTRINSIC を指定して、単眼で求めたK, Dを固定する
    cv::Mat E, F;
    int flags = cv::CALIB_FIX_INTRINSIC; 

    double rms = cv::stereoCalibrate(
        object_points, image_points_L, image_points_R,
        K1, D1,
        K2, D2,
        img_size,
        out_data.R, out_data.T, E, F,
        flags,
        cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-5)
    );

    LOG_INFO("Stereo: 計算完了 RMS = {}", rms);

    // 5. 平行化 (Rectification) 計算
    // ここで Q 行列 (3D復元の要) が生成される
    cv::stereoRectify(
        K1, D1,
        K2, D2,
        img_size,
        out_data.R, out_data.T,
        out_data.R1, out_data.R2, 
        out_data.P1, out_data.P2, 
        out_data.Q,
        cv::CALIB_ZERO_DISPARITY, -1, img_size
    );

    // 6. リマップデータの生成
    // これを保存しておけば、リアルタイム処理時は initUndistortRectifyMap を呼ばなくて済む
    cv::initUndistortRectifyMap(K1, D1, out_data.R1, out_data.P1, img_size, CV_16SC2, out_data.mapL_x, out_data.mapL_y);
    cv::initUndistortRectifyMap(K2, D2, out_data.R2, out_data.P2, img_size, CV_16SC2, out_data.mapR_x, out_data.mapR_y);

    out_data.valid = true;
    out_data.rms = rms;
    
    return rms;
}

} // namespace calib
