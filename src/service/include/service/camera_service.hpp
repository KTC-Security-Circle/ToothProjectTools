#pragma once

#include "service/camera_result.hpp"
#include "video/camera_manager.hpp"

#include <mutex>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace service::camera
{

/// @brief CameraManagerを使ったcamera resource操作とrole bindingを提供するdomain service。
class CameraService
{
  public:
    /// @brief CameraServiceを構築する。
    /// Args:
    ///   cameras <video::CameraManager&>: camera deviceを管理するmanager。
    /// Return:
    ///   <CameraService>: CameraManagerを参照するcamera service。
    explicit CameraService(video::CameraManager& cameras);

    /// @brief camera deviceをopenし、roleへbindする。
    /// Args:
    ///   camera_id <video::CameraId>: OpenCVへ渡すcamera device index。
    ///   role <const std::string&>: runtime内でcameraを参照するrole名。
    /// Return:
    ///   <CameraResult>: camera open結果。
    CameraResult openCamera(video::CameraId camera_id, const std::string& role);

    /// @brief roleに紐づくcameraをcloseする。
    /// Args:
    ///   role <const std::string&>: close対象camera role名。
    /// Return:
    ///   <CameraResult>: camera close結果。
    CameraResult closeCamera(const std::string& role);

    /// @brief roleに紐づくcamera_idを取得する。
    /// Args:
    ///   role <const std::string&>: 解決対象camera role名。
    /// Return:
    ///   <std::optional<video::CameraId>>: roleがbind済みならCameraManager上のcamera_id。
    std::optional<video::CameraId> resolveCameraId(const std::string& role) const;

    /** @brief roleに対応するCameraの最新timestamp付きframeを取得する。 */
    std::optional<video::FrameSample> latestFrame(video::CameraId camera_id) const;
    std::optional<video::FrameSample> firstFrameAtOrAfter(
        video::CameraId camera_id, std::chrono::steady_clock::time_point timestamp) const;

  private:
    /// @brief camera roleとして使用可能な文字列か判定する。
    /// Args:
    ///   role <const std::string&>: 検証対象のrole名。
    /// Return:
    ///   <bool>: 使用可能ならtrue。
    static bool validRole(const std::string& role);

    /// cameras_ <video::CameraManager&>: camera deviceを管理するmanager。
    video::CameraManager& cameras_;

    /// mutex_ <std::mutex>: camera role binding mapを保護するmutex。
    mutable std::mutex mutex_;
    /// role_to_camera_id_ <std::unordered_map<std::string, video::CameraId>>:
    /// role名からmanager上のcamera_idへのbinding。
    std::unordered_map<std::string, video::CameraId> role_to_camera_id_;
    /// role_to_device_index_ <std::unordered_map<std::string, video::CameraId>>: role名からdevice indexへのbinding。
    std::unordered_map<std::string, video::CameraId> role_to_device_index_;
};

} // namespace service::camera
