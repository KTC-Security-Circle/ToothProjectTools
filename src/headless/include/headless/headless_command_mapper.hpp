#pragma once

#include "cmd/commands.hpp"
#include "common/command_result.hpp"

#include <optional>

namespace control
{
struct ControlMessage;
}

namespace service
{
namespace camera
{
class CameraService;
}
} // namespace service

namespace headless
{

struct CommandMapResult
{
    /// ok <bool>: ControlMessageからcmd::Commandへの変換に成功したか。
    bool ok{false};

    /// command <std::optional<cmd::Command>>: 変換成功時のcommand。
    std::optional<cmd::Command> command;

    /// error <std::optional<common::CommandError>>: 変換失敗時のerror情報。
    std::optional<common::CommandError> error;
};

class HeadlessCommandMapper
{
  public:
    /// @brief CameraServiceのrole bindingを参照してHeadlessCommandMapperを構築する。
    ///
    /// Args:
    ///   camera_service <service::camera::CameraService&>: roleからcamera_idを解決するdomain service。
    ///
    /// Return:
    ///   <HeadlessCommandMapper>: CameraService参照を保持するmapper。
    explicit HeadlessCommandMapper(service::camera::CameraService& camera_service);

    /// @brief open_camera用ControlMessageをCmdOpenCameraへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapOpenCamera(const control::ControlMessage& message);

    /// @brief close_camera用ControlMessageをCmdCloseCameraへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCloseCamera(const control::ControlMessage& message);

    /// @brief open_window用ControlMessageをCmdOpenWindowへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapOpenWindow(const control::ControlMessage& message);

    /// @brief close_window用ControlMessageをCmdCloseWindowへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCloseWindow(const control::ControlMessage& message);

    /// @brief list_monitors用ControlMessageをCmdListMonitorsへ変換する。
    CommandMapResult mapListMonitors(const control::ControlMessage& message);

    /// @brief configure_projector_surface用ControlMessageをCmdConfigureProjectorSurfaceへ変換する。
    CommandMapResult mapConfigureProjectorSurface(const control::ControlMessage& message);

    /// @brief open_projector用ControlMessageをCmdOpenProjectorへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapOpenProjector(const control::ControlMessage& message);

    /// @brief close_projector用ControlMessageをCmdCloseProjectorへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCloseProjector(const control::ControlMessage& message);

    /// @brief generate_patterns用ControlMessageをCmdGeneratePatternsへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapGeneratePatterns(const control::ControlMessage& message);

    /// @brief show_pattern用ControlMessageをCmdProjectorShowPatternへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapProjectorShowPattern(const control::ControlMessage& message);

    /// @brief next_pattern用ControlMessageをCmdProjectorNextPatternへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapProjectorNextPattern(const control::ControlMessage& message);

    /// @brief prev_pattern用ControlMessageをCmdProjectorPrevPatternへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapProjectorPrevPattern(const control::ControlMessage& message);

    /// @brief scan_start用ControlMessageをCmdStartScanへ変換する。
    CommandMapResult mapStartScan(const control::ControlMessage& message);

    /// @brief scan_status用ControlMessageをCmdScanStatusへ変換する。
    CommandMapResult mapScanStatus(const control::ControlMessage& message);

    /// @brief scan_stop用ControlMessageをCmdStopScanへ変換する。
    CommandMapResult mapStopScan(const control::ControlMessage& message);

    /// @brief scan_validate用ControlMessageをCmdValidateScanDatasetへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapValidateScanDataset(const control::ControlMessage& message);

    /// @brief decode_patterns用ControlMessageをCmdDecodePatternsへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapDecodePatterns(const control::ControlMessage& message);

    /// @brief capture_frame用ControlMessageをCmdCaptureFrameへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCaptureFrame(const control::ControlMessage& message);

    /// @brief capture_stereo用ControlMessageをCmdCaptureStereoへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCaptureStereo(const control::ControlMessage& message);

    /// @brief calib_capture_frame用ControlMessageをCmdCaptureFrameへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCalibrationCaptureFrame(const control::ControlMessage& message);

    /// @brief calib_capture_stereo用ControlMessageをCmdCaptureStereoへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapCalibrationCaptureStereo(const control::ControlMessage& message);

    CommandMapResult mapDetectCalibrationCorners(const control::ControlMessage& message);
    CommandMapResult mapDetectStereoCalibrationCorners(const control::ControlMessage& message);

    /// @brief mono_calibrate用ControlMessageをCmdCalibrateへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapMonoCalibrate(const control::ControlMessage& message);

    /// @brief stereo_calibrate用ControlMessageをCmdStereoCalibrateへ変換する。
    ///
    /// Args:
    ///   message <const control::ControlMessage&>: JSON Linesからparseされたcontrol message。
    ///
    /// Return:
    ///   <CommandMapResult>: 変換成功時のcmd::Command、失敗時のerror。
    CommandMapResult mapStereoCalibrate(const control::ControlMessage& message);
    CommandMapResult mapValidateReconstruction(const control::ControlMessage& message);
    CommandMapResult mapReconstructPointCloud(const control::ControlMessage& message);

  private:
    /// camera_service_ <service::camera::CameraService&>: role bindingを保持するdomain service。
    service::camera::CameraService& camera_service_;
};

} // namespace headless
