#include "calibration/calibrator.hpp"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>

namespace calib {

Calibrator::Calibrator() {}

void Calibrator::setBoardConfig(const BoardConfig& config) {
    config_ = config;
}

bool Calibrator::detectAndDraw(const cv::Mat& input_img, cv::Mat& output_vis, std::vector<cv::Point2f>& found_points) {
    if (input_img.empty()) return false;

    // 検出用にグレイスケール化（入力がカラーなら）
    cv::Mat gray;
    if (input_img.channels() == 3) {
        cv::cvtColor(input_img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = input_img;
    }

    // コーナー検出
    // CALIB_CB_ADAPTIVE_THRESH: 画像の明るさが不均一でも検出しやすくする
    // CALIB_CB_NORMALIZE_IMAGE: 明るさを正規化してから検出する
    // CALIB_CB_FAST_CHECK: コーナーがない場合に素早く諦める（CPU負荷低減）
    int flags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK;
    
    bool found = cv::findChessboardCorners(gray, config_.pattern_size, found_points, flags);

    // 描画用にカラー変換
    if (input_img.channels() == 1) {
        cv::cvtColor(input_img, output_vis, cv::COLOR_GRAY2BGR);
    } else {
        output_vis = input_img.clone();
    }

    // サブピクセル精度（より高精度な座標）へ最適化
    if (found) {
        cv::cornerSubPix(gray, found_points, cv::Size(11, 11), cv::Size(-1, -1),
            cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1));
    }

    // 描画 (検出できていれば虹色の線、できていなければ何も描かない or 赤い点など)
    cv::drawChessboardCorners(output_vis, config_.pattern_size, found_points, found);

    return found;
}

double Calibrator::runCalibration(
    const std::vector<std::string>& image_files,
    cv::Mat& out_camera_matrix,
    cv::Mat& out_dist_coeffs
) {
    if (image_files.empty()) return -1.0;

    // 1. チェッカーボードの定義 (3D座標: Z=0)
    // (0,0,0), (1,0,0), ..., (cols-1, rows-1, 0) * square_size
    std::vector<cv::Point3f> objp;
    for (int i = 0; i < config_.pattern_size.height; i++) {
        for (int j = 0; j < config_.pattern_size.width; j++) {
            objp.emplace_back(j * config_.square_size_mm, i * config_.square_size_mm, 0.0f);
        }
    }

    std::vector<std::vector<cv::Point3f>> object_points; // 全画像の3D点
    std::vector<std::vector<cv::Point2f>> image_points;  // 全画像の2D点
    cv::Size img_size;

    int success_count = 0;

    // 2. 画像を読み込んでコーナー検出
    for (const auto& fpath : image_files) {
        cv::Mat img = cv::imread(fpath);
        if (img.empty()) continue;

        if (img_size.area() == 0) {
            img_size = img.size();
        } else if (img_size != img.size()) {
            // サイズが不揃いだとエラーになるのでスキップ
            continue;
        }

        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

        std::vector<cv::Point2f> corners;
        // detectAndDraw でも使っているフラグ
        int flags = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK;
        
        bool found = cv::findChessboardCorners(gray, config_.pattern_size, corners, flags);

        if (found) {
            // サブピクセル精度向上
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1));

            image_points.push_back(corners);
            object_points.push_back(objp);
            success_count++;
        }
    }

    if (success_count < 5) {
        // 画像枚数が少なすぎると精度が出ない
        return -1.0; 
    }

    // 3. キャリブレーション計算
    std::vector<cv::Mat> rvecs, tvecs;
    
    // 初期値フラグなし（標準的な設定）
    double rms = cv::calibrateCamera(
        object_points, image_points, img_size,
        out_camera_matrix, out_dist_coeffs,
        rvecs, tvecs
    );

    return rms;
}

} // namespace calib