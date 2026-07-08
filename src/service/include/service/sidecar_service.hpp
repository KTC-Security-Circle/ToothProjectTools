#pragma once

#include "capture/capture_service.hpp"
#include "calibration/calibrator.hpp"
#include "calibration/stereo_calibrator.hpp"
#include "calibration/stereo_data.hpp"
#include "video/camera_manager.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace stream
{
class FramePublisher;
class StreamRegistry;
} // namespace stream

namespace service
{

enum class SidecarErrorCode
{
    InvalidJson,
    InvalidCommand,
    MissingField,
    InvalidRole,
    CameraOpenFailed,
    CameraNotOpen,
    StreamStartFailed,
    CaptureFailed,
    FileWriteFailed,
    InternalError,
};

std::string_view toString(SidecarErrorCode code);

struct SidecarError
{
    SidecarErrorCode code;
    std::string message;
};

struct SidecarResult
{
    bool ok{false};
    std::optional<SidecarError> error;
    std::string value;

    static SidecarResult success(std::string value = {});
    static SidecarResult failure(SidecarErrorCode code, std::string message);
};

class SidecarService
{
  public:
    SidecarService(std::string mjpeg_host, int mjpeg_port, stream::StreamRegistry& streams);
    ~SidecarService();

    SidecarService(const SidecarService&) = delete;
    SidecarService& operator=(const SidecarService&) = delete;
    SidecarService(SidecarService&&) = delete;
    SidecarService& operator=(SidecarService&&) = delete;

    /// @brief cameraをopenしてsidecar roleへ紐づける。
    ///
    /// Args:
    ///   device_index <int>: open対象のdevice index。
    ///   role <const std::string&>: sidecar上でcameraに紐づけるrole名。
    ///
    /// Return:
    ///   <SidecarResult>: openとrole bindingの成否。
    SidecarResult openCamera(int device_index, const std::string& role);

    /// @brief sidecar roleに紐づくcameraをcloseする。
    ///
    /// Args:
    ///   role <const std::string&>: close対象cameraに紐づくrole名。
    ///
    /// Return:
    ///   <SidecarResult>: closeの成否。
    SidecarResult closeCamera(const std::string& role);

    /// @brief sidecar roleに紐づくcameraのMJPEG streamを開始する。
    ///
    /// Args:
    ///   role <const std::string&>: stream開始対象cameraに紐づくrole名。
    ///
    /// Return:
    ///   <SidecarResult>: stream開始の成否と成功時のURL。
    SidecarResult startStream(const std::string& role);

    /// @brief sidecar roleに紐づくcameraのMJPEG streamを停止する。
    ///
    /// Args:
    ///   role <const std::string&>: stream停止対象cameraに紐づくrole名。
    ///
    /// Return:
    ///   <SidecarResult>: stream停止の成否。
    SidecarResult stopStream(const std::string& role);

    /// @brief sidecar roleに紐づくcameraからframeを保存する。
    ///
    /// Args:
    ///   role <const std::string&>: capture対象cameraに紐づくrole名。
    ///   output <const std::string&>: 保存先画像ファイルのpath文字列。
    ///
    /// Return:
    ///   <SidecarResult>: captureと保存の成否、成功時の保存path。
    SidecarResult captureFrame(const std::string& role, const std::string& output);

    /// @brief roleに紐づくcamera_idを取得する。
    ///
    /// Args:
    ///   role <const std::string&>: sidecar上でcameraに紐づけられたrole名。
    ///
    /// Return:
    ///   <std::optional<video::CameraId>>: roleに対応するcamera_id。未登録時はstd::nullopt。
    std::optional<video::CameraId> resolveCameraId(const std::string& role) const;

    /// @brief sidecarが所有するCameraManagerを参照するCaptureServiceを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <capture::CaptureService&>: sidecar camera群へcaptureを実行するdomain service。
    capture::CaptureService& captureService();

    /// @brief sidecarが所有するCameraManagerを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <video::CameraManager&>: sidecar camera群を管理するmanager。
    video::CameraManager& cameraManager();

    /// @brief sidecarが所有するmono calibration計算器を取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <calib::Calibrator*>: mono calibration計算器。
    calib::Calibrator* calibrator();

    /// @brief sidecarが所有するstereo calibration計算器を取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <calib::StereoCalibrator*>: stereo calibration計算器。
    calib::StereoCalibrator* stereoCalibrator();

    /// @brief sidecarが所有するstereo calibration結果を取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <calib::StereoData&>: stereo calibration結果の保存先。
    calib::StereoData& stereoData();

    /// @brief sidecar serviceを停止し、cameraとstreamを解放する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <void>: 戻り値なし。
    void shutdown();

  private:
    struct CameraBinding
    {
        int device_index{-1};
        video::CameraId camera_id{video::kInvalidCameraId};
        std::unique_ptr<stream::FramePublisher> publisher;
    };

    bool validRole(const std::string& role) const;
    std::string streamUrl(const std::string& role) const;

    std::string mjpeg_host_;
    int mjpeg_port_;
    stream::StreamRegistry& streams_;
    video::CameraManager cameras_;
    /// capture_service_ <capture::CaptureService>: sidecar所有camera群を使うcapture用domain service。
    capture::CaptureService capture_service_{cameras_};

    /// calibrator_ <calib::Calibrator>: sidecar用mono calibration計算器。
    calib::Calibrator calibrator_;

    /// stereo_calibrator_ <calib::StereoCalibrator>: sidecar用stereo calibration計算器。
    calib::StereoCalibrator stereo_calibrator_;

    /// stereo_data_ <calib::StereoData>: sidecar用stereo calibration結果。
    calib::StereoData stereo_data_;

    std::map<std::string, CameraBinding> bindings_;
};

} // namespace service
