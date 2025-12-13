// src/video/camera.cpp
// =============================================================================
//  camera.cpp 実装
// =============================================================================

#include "video/camera.hpp"
#include "logger/logger_macros.hpp" // プロジェクトのロガー

#include <opencv2/videoio.hpp>
#include <opencv2/core/mat.hpp>
#include <stdexcept>

namespace video {

// -----------------------------------------------------------------------------
// 内部ヘルパー: ポインタ検証
// -----------------------------------------------------------------------------
namespace {
  template <typename T>
  T& ensure_ptr(T* ptr) {
    if (!ptr) {
      throw std::runtime_error("[Camera] Internal pointer is null (allocation failed?)");
    }
    return *ptr;
  }
}

// -----------------------------------------------------------------------------
// コンストラクタ / デストラクタ
// -----------------------------------------------------------------------------
Camera::Camera(const CameraOptions& options, Id camera_id, const std::string& camera_name)
    : options_(options), id_(camera_id), name_(camera_name)
{
    // PIMPLイディオム（ポインタの実体確保）
    capture_ptr_ = new cv::VideoCapture();
    current_frame_ptr_ = new cv::Mat();
    intrinsics_storage_ptr_ = new cv::Mat();
    distortion_storage_ptr_ = new cv::Mat();
}

Camera::~Camera() {
    close(); // 確実に閉じる
    
    // メモリ解放
    delete capture_ptr_;
    delete current_frame_ptr_;
    delete intrinsics_storage_ptr_;
    delete distortion_storage_ptr_;
}

// -----------------------------------------------------------------------------
// 内部アクセサ（実装ファイル内でのみ使用）
// -----------------------------------------------------------------------------
cv::VideoCapture& Camera::capture_()      { return ensure_ptr(capture_ptr_); }
cv::Mat&          Camera::currentFrame_() { return ensure_ptr(current_frame_ptr_); }
cv::Mat&          Camera::intrinsics_()   { return ensure_ptr(intrinsics_storage_ptr_); }
cv::Mat&          Camera::distortion_()   { return ensure_ptr(distortion_storage_ptr_); }

// -----------------------------------------------------------------------------
// ライフサイクル
// -----------------------------------------------------------------------------
bool Camera::open() {
    if (is_opened_) return true;

    // オプションからデバイスインデックスを取得してオープン
    if (capture_().open(options_.device_index, cv::CAP_V4L2)) {
        is_opened_ = true;

        capture_().set(cv::CAP_PROP_FOURCC, options_.fourcc);
        capture_().set(cv::CAP_PROP_FRAME_WIDTH, options_.width);
        capture_().set(cv::CAP_PROP_FRAME_HEIGHT, options_.height);
        
        if (options_.fps > 0) {
            capture_().set(cv::CAP_PROP_FPS, options_.fps);
        }
        
        // 自動露光設定 (V4L2: 0.25 or 0.75 など環境依存があるため注意)
        if (options_.auto_exposure) {
             capture_().set(cv::CAP_PROP_AUTO_EXPOSURE, 3); // 3=Auto (V4L2)
        } else {
             capture_().set(cv::CAP_PROP_AUTO_EXPOSURE, 1); // 1=Manual (V4L2)
             // 必要であれば exposure_ms 設定を追加
        }

        LOG_INFO("Camera opened: id={}, device={}, name='{}', size={}x{}", 
                 id_, options_.device_index, name_, options_.width, options_.height);
        return true;
    }

    LOG_ERROR("Camera open failed: id={}, device={}, name='{}'", id_, options_.device_index, name_);
    return false;
}

void Camera::close() {
    if (is_opened_ && capture_ptr_) {
        capture_().release();
    }
    is_opened_ = false;
}

// -----------------------------------------------------------------------------
// フレーム取得
// -----------------------------------------------------------------------------
cv::Mat Camera::getFrame() {
    if (!is_opened_) return cv::Mat();

    cv::Mat& frame = currentFrame_();
    if (capture_().read(frame)) {
        last_captured_time_ = std::chrono::steady_clock::now();
        return frame.clone(); // 呼び出し元が加工できるようコピーを返す
    }
    
    LOG_WARN("Camera read failed (empty frame): id={}", id_);
    return cv::Mat();
}

// -----------------------------------------------------------------------------
// キャリブレーション設定
// -----------------------------------------------------------------------------
void Camera::setIntrinsics(const cv::Mat& camera_matrix) {
    if (!camera_matrix.empty()) {
        camera_matrix.copyTo(intrinsics_());
    }
}

void Camera::setDistCoeffs(const cv::Mat& distortion) {
    if (!distortion.empty()) {
        distortion.copyTo(distortion_());
    }
}

// -----------------------------------------------------------------------------
// キャリブレーション取得
// -----------------------------------------------------------------------------
const cv::Mat& Camera::intrinsics() const noexcept {
    return *intrinsics_storage_ptr_;
}

const cv::Mat& Camera::distCoeffs() const noexcept {
    return *distortion_storage_ptr_;
}

} // namespace video