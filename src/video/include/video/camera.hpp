#pragma once
// =============================================================================
//  camera.hpp
//  ワーカースレッド版 カメラ制御
// =============================================================================

#include <string>
#include <chrono>
#include <cstdint>
#include <thread>  // ★追加
#include <mutex>   // ★追加
#include <atomic>  // ★追加

#include "video_types.hpp"

namespace cv { class Mat; class VideoCapture; }

namespace video {
class Camera {
public:
  using Id = std::uint64_t;

  Camera(const CameraOptions& options, Id camera_id, const std::string& camera_name = "Camera");
  ~Camera();

  // ライフサイクル
  bool open();
  void close();

  // 状態
  bool isOpened() const noexcept { return is_opened_; }

  // ★変更: ノンブロッキングで最新フレームのコピーを返す
  cv::Mat getFrame();

  // プロパティ
  Id id() const noexcept { return id_; }
  int index() const noexcept { return options_.device_index; }
  const std::string& name() const noexcept { return name_; }
  int monitorIndex() const noexcept { return monitor_index_; }
  void setMonitorIndex(int monitor_index_value) { monitor_index_ = monitor_index_value; }

  // キャリブレーション
  void setIntrinsics(const cv::Mat& camera_matrix);
  void setDistCoeffs(const cv::Mat& distortion);
  const cv::Mat& intrinsics() const noexcept;
  const cv::Mat& distCoeffs() const noexcept;

private:
  // ★追加: スレッド制御メソッド
  void startThread();
  void stopThread();
  void workerLoop();

private:
  CameraOptions options_;
  Id id_{0};
  std::string name_;
  int monitor_index_{1};

  bool is_opened_{false};
  cv::VideoCapture* capture_ptr_{nullptr};

  // ★変更: スレッド共有リソース
  std::atomic<bool> is_streaming_{false}; // スレッド稼働フラグ
  std::thread worker_thread_;             // 撮影スレッド
  std::mutex frame_mutex_;                // 画像保護用ミューテックス
  cv::Mat last_frame_;                    // 最新フレームキャッシュ（実体）
  
  // キャリブレーション
  cv::Mat* intrinsics_storage_ptr_{nullptr};
  cv::Mat* distortion_storage_ptr_{nullptr};

  // 内部アクセサ
  cv::VideoCapture& capture_();
  cv::Mat&          intrinsics_();
  cv::Mat&          distortion_();
};
}