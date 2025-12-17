// =============================================================================
//  camera.cpp 実装 (ワーカースレッド対応版)
// =============================================================================

#include "video/camera.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/videoio.hpp>
#include <opencv2/core/mat.hpp>
#include <stdexcept>

namespace video {

// -----------------------------------------------------------------------------
// 内部ヘルパー
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
    capture_ptr_ = new cv::VideoCapture();
    // current_frame_ptr_ は廃止し、メンバ変数 last_frame_ を使用します
    intrinsics_storage_ptr_ = new cv::Mat();
    distortion_storage_ptr_ = new cv::Mat();
}

Camera::~Camera() {
    stopThread(); // デストラクタで必ずスレッドを止める
    close();
    
    delete capture_ptr_;
    delete intrinsics_storage_ptr_;
    delete distortion_storage_ptr_;
}

// -----------------------------------------------------------------------------
// 内部アクセサ
// -----------------------------------------------------------------------------
cv::VideoCapture& Camera::capture_()      { return ensure_ptr(capture_ptr_); }
cv::Mat&          Camera::intrinsics_()   { return ensure_ptr(intrinsics_storage_ptr_); }
cv::Mat&          Camera::distortion_()   { return ensure_ptr(distortion_storage_ptr_); }

// -----------------------------------------------------------------------------
// ライフサイクル
// -----------------------------------------------------------------------------
bool Camera::open() {
    if (is_opened_) return true;

    if (capture_().open(options_.device_index, cv::CAP_V4L2)) {
        is_opened_ = true;

        capture_().set(cv::CAP_PROP_FOURCC, options_.fourcc);
        capture_().set(cv::CAP_PROP_FRAME_WIDTH, options_.width);
        capture_().set(cv::CAP_PROP_FRAME_HEIGHT, options_.height);
        
        if (options_.fps > 0) {
            capture_().set(cv::CAP_PROP_FPS, options_.fps);
        }
        
        if (options_.auto_exposure) {
             capture_().set(cv::CAP_PROP_AUTO_EXPOSURE, 3);
        } else {
             capture_().set(cv::CAP_PROP_AUTO_EXPOSURE, 1);
             // capture_().set(cv::CAP_PROP_EXPOSURE, ...);
        }

        LOG_INFO("Camera opened: id={}, device={}, name='{}'", id_, options_.device_index, name_);
        
        // ★オープン成功後にスレッド開始
        startThread();
        
        return true;
    }

    LOG_ERROR("Camera open failed: id={}, device={}, name='{}'", id_, options_.device_index, name_);
    return false;
}

void Camera::close() {
    stopThread(); // ★クローズ前にスレッド停止
    
    if (is_opened_ && capture_ptr_) {
        capture_().release();
    }
    is_opened_ = false;
}

// -----------------------------------------------------------------------------
// ★スレッド制御の実装
// -----------------------------------------------------------------------------
void Camera::startThread() {
    if (is_streaming_) return;
    
    // スレッド起動
    is_streaming_ = true;
    worker_thread_ = std::thread(&Camera::workerLoop, this);
    LOG_INFO("Camera Worker Started: {}", name_);
}

void Camera::stopThread() {
    if (!is_streaming_) return;

    // スレッド停止要求
    is_streaming_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join(); // 終了待ち
    }
    LOG_INFO("Camera Worker Stopped: {}", name_);
}

void Camera::workerLoop() {
    cv::Mat temp_frame; // スレッドローカルなバッファ
    
    while (is_streaming_) {
        if (!capture_ptr_ || !capture_ptr_->isOpened()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 1. 撮影 (ここで33ms待たされるが、UIスレッドではないのでOK)
        if (capture_ptr_->read(temp_frame) && !temp_frame.empty()) {
            
            // 2. 最新フレームを保護しながら更新 (一瞬で終わる)
            std::lock_guard<std::mutex> lock(frame_mutex_);
            temp_frame.copyTo(last_frame_);
            
        } else {
            // エラー時はCPU負荷を下げるため少し待つ
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

// -----------------------------------------------------------------------------
// フレーム取得 (ノンブロッキング)
// -----------------------------------------------------------------------------
cv::Mat Camera::getFrame() {
    // ミューテックスで保護された最新フレームを取りに行く
    // 待ち時間はメモリコピーの時間だけ（<1ms）
    std::lock_guard<std::mutex> lock(frame_mutex_);
    
    if (last_frame_.empty()) {
        // まだ1枚も撮れていない場合
        return cv::Mat();
    }
    
    return last_frame_.clone();
}

// -----------------------------------------------------------------------------
// キャリブレーション
// -----------------------------------------------------------------------------
void Camera::setIntrinsics(const cv::Mat& camera_matrix) {
    if (!camera_matrix.empty()) camera_matrix.copyTo(intrinsics_());
}

void Camera::setDistCoeffs(const cv::Mat& distortion) {
    if (!distortion.empty()) distortion.copyTo(distortion_());
}

const cv::Mat& Camera::intrinsics() const noexcept {
    return *intrinsics_storage_ptr_;
}

const cv::Mat& Camera::distCoeffs() const noexcept {
    return *distortion_storage_ptr_;
}

} // namespace video