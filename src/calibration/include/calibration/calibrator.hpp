#pragma once
#include <opencv2/core.hpp>
#include <vector>

namespace calib {

struct BoardConfig {
    cv::Size pattern_size{10, 7}; // 交点の数 (行, 列) ※マス目の数ではない注意
    float square_size_mm{24.0f};  // マス目のサイズ
};

class Calibrator {
public:
    Calibrator();
    ~Calibrator() = default;

    // 設定
    void setBoardConfig(const BoardConfig& config);

    // チェッカーボード検出＆描画
    // input_img: 元画像
    // output_vis: 描画後の画像（検出できた場合、虹色の線が引かれる）
    // found_points: 見つかった座標の格納先
    // 戻り値: 検出できたら true
    bool detectAndDraw(const cv::Mat& input_img, cv::Mat& output_vis, std::vector<cv::Point2f>& found_points);

    double runCalibration(
        const std::vector<std::string>& image_files,
        cv::Mat& out_camera_matrix,
        cv::Mat& out_dist_coeffs
    );

private:
    BoardConfig config_;
};

} // namespace calib