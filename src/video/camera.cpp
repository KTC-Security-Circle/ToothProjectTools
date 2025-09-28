// =============================================================================
//  camera.cpp
//  Camera クラスの実装。重い OpenCV ヘッダはここに限定する。
// =============================================================================

#include "video/camera.hpp"
#include "logger/logger_macros.hpp"

#include <opencv2/videoio.hpp>   // VideoCapture
#include <opencv2/core/mat.hpp>  // cv::Mat
#include <utility>

namespace {

// ヒープ確保の薄いヘルパ（例外安全簡易化のため）
template <typename T>
T& ensure_ptr(T*& ptr) {
  if (!ptr) ptr = new T();
  return *ptr;
}
template <typename T>
void release_ptr(T*& ptr) {
  delete ptr;
  ptr = nullptr;
}

// const 参照用の空行列（確保しない方針の const getter 向け）
const cv::Mat& empty_mat() {
  static const cv::Mat kEmpty;
  return kEmpty;
}

} // namespace

// -----------------------------------------------------------------------------
// コンストラクタ
// -----------------------------------------------------------------------------
Camera::Camera(int device_index, Id camera_id, const std::string& camera_name)
  : device_index_(device_index),
    id_(camera_id),
    name_(camera_name) {}

// 内部参照（非constのみ確保）
cv::VideoCapture& Camera::capture_()      { return ensure_ptr(capture_ptr_); }
cv::Mat&          Camera::currentFrame_() { return ensure_ptr(current_frame_ptr_); }
cv::Mat&          Camera::intrinsics_()   { return ensure_ptr(intrinsics_storage_ptr_); }
cv::Mat&          Camera::distortion_()   { return ensure_ptr(distortion_storage_ptr_); }

// -----------------------------------------------------------------------------
// デバイスを開く
// -----------------------------------------------------------------------------
bool Camera::open() {
  if (is_opened_) return true;

  cv::VideoCapture& cap = capture_();

  // V4L2 を優先（Linux 想定）。失敗時は既定バックエンドへフォールバック。
  if (!cap.open(device_index_, cv::CAP_V4L2)) {
    LOG_WARN("VideoCapture(V4L2) でオープンに失敗: index={}", device_index_);
    if (!cap.open(device_index_)) {
      LOG_ERROR("VideoCapture でオープンに失敗: index={}", device_index_);
      is_opened_ = false;
      return false;
    }
  }

  is_opened_ = true;
  LOG_INFO("Camera をオープン: index={}, id={}, name='{}'",
           device_index_, id_, name_);
  return true;
}

// -----------------------------------------------------------------------------
// デバイスを閉じる
// -----------------------------------------------------------------------------
void Camera::close() {
  if (!is_opened_) return;

  if (capture_ptr_) {
    if (capture_ptr_->isOpened()) capture_ptr_->release();
  }
  is_opened_ = false;
  LOG_INFO("Camera をクローズ: index={}, id={}, name='{}'",
           device_index_, id_, name_);
}

// -----------------------------------------------------------------------------
// フレーム取得
// -----------------------------------------------------------------------------
cv::Mat Camera::getFrame() {
  cv::Mat& current = currentFrame_();

  if (capture_ptr_ && capture_ptr_->isOpened()) {
    cv::Mat grabbed;
    (*capture_ptr_) >> grabbed; // 1フレーム取得
    if (!grabbed.empty()) {
      current = grabbed.clone();
      last_captured_time_ = std::chrono::steady_clock::now();
    }
  }
  // 最新フレーム（空の可能性あり）を返す
  return current;
}

// -----------------------------------------------------------------------------
// キャリブレーション：内部行列
// -----------------------------------------------------------------------------
void Camera::setIntrinsics(const cv::Mat& camera_matrix) {
  intrinsics_() = camera_matrix.clone();
}

// -----------------------------------------------------------------------------
// キャリブレーション：歪み係数
// -----------------------------------------------------------------------------
void Camera::setDistCoeffs(const cv::Mat& distortion) {
  distortion_() = distortion.clone();
}

// -----------------------------------------------------------------------------
// キャリブレーション取得（const）
// -----------------------------------------------------------------------------
const cv::Mat& Camera::intrinsics() const noexcept {
  return intrinsics_storage_ptr_ ? *intrinsics_storage_ptr_ : empty_mat();
}
const cv::Mat& Camera::distCoeffs() const noexcept {
  return distortion_storage_ptr_ ? *distortion_storage_ptr_ : empty_mat();
}
