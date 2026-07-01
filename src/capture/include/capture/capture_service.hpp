#pragma once

#include "capture/capture_result.hpp"
#include "video/video_types.hpp"

#include <filesystem>
#include <optional>

namespace video
{
class CameraManager;
}

namespace capture
{

/// @brief Cameraからframeを取得し、画像ファイルとして保存するdomain service。
class CaptureService
{
public:
    /// @brief CameraManagerを参照してCaptureServiceを構築する。
    ///
    /// Args:
    ///   cameras <video::CameraManager&>: Capture対象cameraを管理するCameraManager。
    ///
    /// Return:
    ///   <CaptureService>: CameraManagerを参照するCaptureService。
    explicit CaptureService(video::CameraManager& cameras);

    CaptureService(const CaptureService&) = delete;
    CaptureService& operator=(const CaptureService&) = delete;
    CaptureService(CaptureService&&) = delete;
    CaptureService& operator=(CaptureService&&) = delete;

    /// @brief 指定cameraからframeを取得し、画像ファイルとして保存する。
    ///
    /// Args:
    ///   camera_id <video::CameraId>: Capture対象のcamera識別子。
    ///   output_path <const std::filesystem::path&>: 保存先画像ファイルのpath。
    ///
    /// Return:
    ///   <CaptureResult>: Captureと保存の成否、失敗時のerror、成功時の保存path。
    CaptureResult captureFrame(video::CameraId camera_id, const std::filesystem::path& output_path);

    /// @brief 左右cameraから近いタイミングでframeを取得し、それぞれ画像ファイルとして保存する。
    ///
    /// Args:
    ///   left_camera_id <video::CameraId>: 左側Capture対象のcamera識別子。
    ///   right_camera_id <video::CameraId>: 右側Capture対象のcamera識別子。
    ///   left_output_path <const std::filesystem::path&>: 左camera画像ファイルの保存先path。
    ///   right_output_path <const std::filesystem::path&>: 右camera画像ファイルの保存先path。
    ///
    /// Return:
    ///   <CaptureStereoResult>: 左右Captureと保存の成否、失敗時のerror、成功時の保存path。
    CaptureStereoResult captureStereo(video::CameraId left_camera_id,
                                      video::CameraId right_camera_id,
                                      const std::filesystem::path& left_output_path,
                                      const std::filesystem::path& right_output_path);

private:
    /// @brief 保存先pathの親ディレクトリを作成する。
    ///
    /// Args:
    ///   output_path <const std::filesystem::path&>: 保存対象ファイルのpath。
    ///
    /// Return:
    ///   <std::optional<CaptureError>>: 作成成功時はstd::nullopt、失敗時はerror。
    std::optional<CaptureError> ensureParentDirectory(const std::filesystem::path& output_path) const;

    /// cameras_ <video::CameraManager&>: Capture対象cameraを取得するCameraManager。
    video::CameraManager& cameras_;
};

} // namespace capture
