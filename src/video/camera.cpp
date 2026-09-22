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
    worker_thread_ = std::thread([this]() {
        try {
            workerLoop();
        } catch (const cv::Exception& e) {
            LOG_ERROR("Camera Worker OpenCV exception: name={}, error={}", name_, e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Camera Worker exception: name={}, error={}", name_, e.what());
        } catch (...) {
            LOG_ERROR("Camera Worker unknown exception: name={}", name_);
        }
    });
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
    std::uint64_t consecutive_read_failures = 0;
    
    while (is_streaming_) {
        if (!capture_ptr_ || !capture_ptr_->isOpened()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 1. 撮影 (ここで33ms待たされるが、UIスレッドではないのでOK)
        bool read_ok = false;
        bool read_threw = false;
        try {
            read_ok = capture_ptr_->read(temp_frame);
        } catch (const cv::Exception& e) {
            read_threw = true;
            ++consecutive_read_failures;
            if (consecutive_read_failures == 1 || consecutive_read_failures % 100 == 0) {
                LOG_WARN("Camera read OpenCV exception: name={}, consecutive_failures={}, error={}",
                         name_, consecutive_read_failures, e.what());
            }
        } catch (const std::exception& e) {
            read_threw = true;
            ++consecutive_read_failures;
            if (consecutive_read_failures == 1 || consecutive_read_failures % 100 == 0) {
                LOG_WARN("Camera read exception: name={}, consecutive_failures={}, error={}",
                         name_, consecutive_read_failures, e.what());
            }
        }

        if (!read_ok || temp_frame.empty()) {
            if (read_ok) {
                ++consecutive_read_failures;
                if (consecutive_read_failures == 1 || consecutive_read_failures % 100 == 0) {
                    LOG_WARN("Camera read returned an empty frame: name={}, consecutive_failures={}",
                             name_, consecutive_read_failures);
                }
            } else if (!read_threw) {
                ++consecutive_read_failures;
                if (consecutive_read_failures == 1 || consecutive_read_failures % 100 == 0) {
                    LOG_WARN("Camera read failed: name={}, consecutive_failures={}",
                             name_, consecutive_read_failures);
                }
            }

            // エラー時はCPU負荷を下げるため少し待つ
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        consecutive_read_failures = 0;

        // read成功直後のhost時刻を記録する。hardware timestampとは呼ばない。
        const auto capture_timestamp = std::chrono::steady_clock::now();

        // 2. 正常なフレームだけを保護しながら更新 (一瞬で終わる)
        std::lock_guard<std::mutex> lock(frame_mutex_);
        temp_frame.copyTo(last_frame_);
        FrameSample sample;
        sample.image = temp_frame.clone();
        sample.sequence = ++next_frame_sequence_;
        sample.timestamp = capture_timestamp;
        frame_ring_.push_back(std::move(sample));
        while (frame_ring_.size() > frame_ring_capacity_) {
            frame_ring_.pop_front();
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

std::optional<FrameSample> Camera::getFrameSample() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (frame_ring_.empty()) return std::nullopt;
    FrameSample result = frame_ring_.back();
    result.image = result.image.clone();
    return result;
}

std::optional<FrameSample> Camera::firstFrameAtOrAfter(
    std::chrono::steady_clock::time_point timestamp) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return video::firstFrameAtOrAfter(frame_ring_, timestamp);
}

void Camera::setFrameRingCapacity(std::size_t capacity) {
    if (capacity == 0) capacity = 1;
    std::lock_guard<std::mutex> lock(frame_mutex_);
    frame_ring_capacity_ = capacity;
    while (frame_ring_.size() > frame_ring_capacity_) frame_ring_.pop_front();
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
