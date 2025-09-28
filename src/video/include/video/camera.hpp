#pragma once
// =============================================================================
//  camera.hpp
//  単純なカメラ制御（VideoCapture のラッパ）
//  - ライフサイクル: open/close
//  - フレーム取得: getFrame
//  - 基本プロパティ: id/index/name/monitor_index
//  - キャリブレーション: 内部行列・歪み係数の保管
//  ヘッダは軽量化のため OpenCV の重いヘッダを含めない。
// =============================================================================

#include <string>
#include <chrono>
#include <cstdint>

namespace cv { class Mat; class VideoCapture; }

class Camera {
public:
  using Id = std::uint64_t;

  // コンストラクタ
  Camera(int device_index, Id camera_id, const std::string& camera_name = "Camera");

  // ライフサイクル
  bool open();
  void close();

  // 状態
  bool isOpened() const noexcept { return is_opened_; }

  // フレーム取得：最新フレームのコピーを返す（空の可能性あり）
  cv::Mat getFrame();

  // --- 単純プロパティ（ドキュメント省略） ---
  Id id() const noexcept { return id_; }
  int index() const noexcept { return device_index_; }
  const std::string& name() const noexcept { return name_; }
  int monitorIndex() const noexcept { return monitor_index_; }
  void setMonitorIndex(int monitor_index_value) { monitor_index_ = monitor_index_value; }

  // キャリブレーション（setter はコピー保存）
  void setIntrinsics(const cv::Mat& camera_matrix);
  void setDistCoeffs(const cv::Mat& distortion);

  // キャリブレーション取得（const 参照・空なら空行列を返す）
  const cv::Mat& intrinsics() const noexcept;
  const cv::Mat& distCoeffs() const noexcept;

private:
  int device_index_{0};
  Id id_{0};
  std::string name_;
  int monitor_index_{1};

  bool is_opened_{false};
  cv::VideoCapture* capture_ptr_{nullptr};

  cv::Mat* current_frame_ptr_{nullptr};
  std::chrono::steady_clock::time_point last_captured_time_;

  // キャリブレーション
  cv::Mat* intrinsics_storage_ptr_{nullptr};
  cv::Mat* distortion_storage_ptr_{nullptr};

  // 実体は .cpp 側で確保
  cv::VideoCapture& capture_();
  cv::Mat&          currentFrame_();
  cv::Mat&          intrinsics_();
  cv::Mat&          distortion_();
};
