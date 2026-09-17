#pragma once

#include "video/video_types.hpp"

#include <optional>
#include <string>

namespace video
{

/// @brief camera commandのerror情報。
struct CameraError
{
    /// code <std::string>: camera command失敗時のerror code。
    std::string code;

    /// message <std::string>: camera command失敗時のerror message。
    std::string message;
};

/// @brief camera open/closeの実行結果。
struct CameraResult
{
    /// ok <bool>: camera commandが成功したか。
    bool ok{false};

    /// camera_id <video::CameraId>: 対象camera device index。
    video::CameraId camera_id{video::kInvalidCameraId};

    /// role <std::string>: 対象camera role名。
    std::string role;

    /// error <std::optional<CameraError>>: 失敗時のerror情報。
    std::optional<CameraError> error;

    /// @brief camera commandの成功resultを作成する。
    ///
    /// Args:
    ///   camera_id <video::CameraId>: 対象camera device index。
    ///   role <std::string>: 対象camera role名。
    ///
    /// Return:
    ///   <CameraResult>: ok=trueのcamera command result。
    static CameraResult success(video::CameraId camera_id, std::string role);

    /// @brief camera commandの失敗resultを作成する。
    ///
    /// Args:
    ///   camera_id <video::CameraId>: 対象camera device index。不明な場合はkInvalidCameraId。
    ///   role <std::string>: 対象camera role名。
    ///   code <std::string>: camera command失敗時のerror code。
    ///   message <std::string>: camera command失敗時のerror message。
    ///
    /// Return:
    ///   <CameraResult>: ok=falseのcamera command result。
    static CameraResult failure(video::CameraId camera_id, std::string role, std::string code, std::string message);
};

} // namespace video
