#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>

class Camera {
public:
    using Id = std::uint64_t;

    // --- コンストラクタ ---
    Camera(int index, Id id, const std::string& name = "Camera")
        : index_(index), id_(id), name_(name) {}

    // --- ライフサイクル ---
    bool open() {
        if (cap_.isOpened()) return true;
        if (!cap_.open(index_, cv::CAP_V4L2)) {
            return false;
        }
        is_opened_ = true;
        return true;
    }

    void close() {
        if (cap_.isOpened()) {
            cap_.release();
        }
        is_opened_ = false;
    }

    bool isOpened() const noexcept { return is_opened_; }

    // --- フレーム取得 ---
    cv::Mat getFrame() {
        cv::Mat frame;
        if (cap_.isOpened()) {
            cap_ >> frame; // 1フレーム取得
            if (!frame.empty()) {
                current_frame_ = frame.clone();
                last_captured_time_ = std::chrono::steady_clock::now();
            }
        }
        return current_frame_;
    }

    // --- プロパティ getter ---
    Id id() const noexcept { return id_; }
    int index() const noexcept { return index_; }
    const std::string& name() const noexcept { return name_; }
    int monitorIndex() const noexcept { return monitor_index_; }

    // --- プロパティ setter ---
    void setMonitorIndex(int idx) { monitor_index_ = idx; }

    // キャリブレーション関連
    void setIntrinsics(const cv::Mat& K) { intrinsics_ = K.clone(); }
    void setDistCoeffs(const cv::Mat& d) { distortion_coeffs_ = d.clone(); }

    const cv::Mat& intrinsics() const { return intrinsics_; }
    const cv::Mat& distCoeffs() const { return distortion_coeffs_; }

private:
    int index_{0};                       // VideoCapture インデックス
    Id id_{0};                           // アプリ内ユニークID
    std::string name_;                   // 論理名
    int monitor_index_{1};               // Windowとの紐付けモニタ番号

    bool is_opened_{false};
    cv::VideoCapture cap_;               // 実際のキャプチャデバイス

    cv::Mat current_frame_;              // 最新フレーム
    std::chrono::steady_clock::time_point last_captured_time_;

    // キャリブレーション
    cv::Mat intrinsics_;
    cv::Mat distortion_coeffs_;
};
