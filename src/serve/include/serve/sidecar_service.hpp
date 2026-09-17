#pragma once

#include "calibration/calibrator.hpp"
#include "calibration/stereo_calibrator.hpp"
#include "calibration/stereo_data.hpp"
#include "capture/capture_service.hpp"
#include "reconstruction/reconstruction_service.hpp"
#include "video/camera_service.hpp"
#include "decode/decode_service.hpp"
#include "window/monitor_service.hpp"
#include "projector/projector_service.hpp"
#include "scan/scan_dataset_validator.hpp"
#include "scan/scan_event.hpp"
#include "scan/scan_service.hpp"
#include "window/window_service.hpp"
#include "video/camera_manager.hpp"
#include "window/window_manager.hpp"

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

namespace serve
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
    ///   device_index <video::CameraId>: open対象のdevice index。
    ///   role <const std::string&>: sidecar上でcameraに紐づけるrole名。
    ///
    /// Return:
    ///   <SidecarResult>: openとrole bindingの成否。
    video::CameraResult openCamera(video::CameraId device_index, const std::string& role);

    /// @brief sidecar roleに紐づくcameraをcloseする。
    ///
    /// Args:
    ///   role <const std::string&>: close対象cameraに紐づくrole名。
    ///
    /// Return:
    ///   <SidecarResult>: closeの成否。
    video::CameraResult closeCamera(const std::string& role);

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

    /// @brief 起動中ならsidecar roleに紐づくMJPEG streamを停止する。
    ///
    /// Args:
    ///   role <const std::string&>: stream停止対象cameraに紐づくrole名。
    ///
    /// Return:
    ///   <void>: 未起動streamは成功扱いとして無視する。
    void stopStreamIfRunning(const std::string& role);

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

    /// @brief camera resource commandを実行するdomain serviceを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <video::CameraService&>: sidecar所有CameraManagerを使うcamera service。
    video::CameraService& cameraService();

    /// @brief window resource commandを実行するdomain serviceを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <win::WindowService&>: sidecar所有WindowManagerを使うwindow service。
    win::WindowService& windowService();

    /// @brief monitor情報を取得するdomain serviceを取得する。
    win::MonitorService& monitorService();

    /// @brief projector commandを実行するdomain serviceを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <projector::ProjectorService&>: sidecar所有WindowServiceを使うprojector service。
    projector::ProjectorService& projectorService();

    /// @brief sidecarが所有するCameraManagerを参照するCaptureServiceを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <capture::CaptureService&>: sidecar camera群へcaptureを実行するdomain service。
    capture::CaptureService& captureService();

    /// @brief scan commandを実行するdomain serviceを取得する。
    scan::ScanService& scanService();

    /// @brief scan dataset検証serviceを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <scan::dataset::ScanDatasetValidator&>: sidecar所有scan dataset validator。
    scan::dataset::ScanDatasetValidator& scanDatasetValidator();

    /// @brief GrayCode decode serviceを取得する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <decode::DecodeService&>: sidecar所有decode service。
    decode::DecodeService& decodeService();

    /// @brief scan worker event queueを取得する。
    scan::ScanEventQueue& scanEventQueue();

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
    reconstruction::ReconstructionService& reconstructionService();

    /// @brief sidecar serviceを停止し、cameraとstreamを解放する。
    ///
    /// Args:
    ///   none <void>: 引数なし。
    ///
    /// Return:
    ///   <void>: 戻り値なし。
    void shutdown();

  private:
    /// @brief roleごとのMJPEG publisherを保持するsidecar固有binding。
    struct CameraBinding
    {
        /// publisher <std::unique_ptr<stream::FramePublisher>>: roleのframe配信worker。
        std::unique_ptr<stream::FramePublisher> publisher;
    };

    /// @brief role用MJPEG stream URLを生成する。
    /// Args:
    ///   role <const std::string&>: URLへ埋め込むcamera role名。
    /// Return:
    ///   <std::string>: role用MJPEG stream URL。
    std::string streamUrl(const std::string& role) const;

    std::string mjpeg_host_;
    int mjpeg_port_;
    stream::StreamRegistry& streams_;
    video::CameraManager cameras_;

    /// window_manager_ <win::WindowManager>: sidecar window resourceを管理するmanager。
    win::WindowManager window_manager_;

    /// camera_service_ <video::CameraService>: camera resourceとrole bindingを管理するdomain service。
    video::CameraService camera_service_{cameras_};

    /// monitor_service_ <win::MonitorService>: monitor情報を取得するdomain service。
    win::MonitorService monitor_service_;

    /// window_service_ <win::WindowService>: window resourceとrole bindingを管理するdomain service。
    win::WindowService window_service_{window_manager_, monitor_service_};

    /// projector_service_ <projector::ProjectorService>: projector roleとpattern表示を管理するdomain service。
    projector::ProjectorService projector_service_{window_service_, monitor_service_};

    /// capture_service_ <capture::CaptureService>: sidecar所有camera群を使うcapture用domain service。
    capture::CaptureService capture_service_{cameras_};

    /// scan_event_queue_ <scan::ScanEventQueue>: scan worker event queue。
    scan::ScanEventQueue scan_event_queue_;

    /// scan_service_ <scan::ScanService>: 自動構造光scan domain service。
    scan::ScanService scan_service_{projector_service_, capture_service_, camera_service_, scan_event_queue_};

    /// scan_dataset_validator_ <scan::dataset::ScanDatasetValidator>: scan dataset検証service。
    scan::dataset::ScanDatasetValidator scan_dataset_validator_;

    /// decode_service_ <decode::DecodeService>: scan datasetからGrayCode decode結果を生成するservice。
    decode::DecodeService decode_service_{scan_dataset_validator_};

    /// calibrator_ <calib::Calibrator>: sidecar用mono calibration計算器。
    calib::Calibrator calibrator_;

    /// stereo_calibrator_ <calib::StereoCalibrator>: sidecar用stereo calibration計算器。
    calib::StereoCalibrator stereo_calibrator_;

    /// stereo_data_ <calib::StereoData>: sidecar用stereo calibration結果。
    calib::StereoData stereo_data_;
    reconstruction::ReconstructionService reconstruction_service_;

    /// bindings_ <std::map<std::string, CameraBinding>>: roleごとのMJPEG publisherを保持するsidecar binding。
    std::map<std::string, CameraBinding> bindings_;
};

} // namespace serve
