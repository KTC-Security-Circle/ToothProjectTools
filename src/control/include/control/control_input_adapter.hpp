#pragma once

#include "control/control_message.hpp"
#include "headless/headless_command_mapper.hpp"
#include "headless/headless_command_executor.hpp"

namespace serve {
class SidecarService;
struct SidecarResult;
}

namespace common {
struct CommandError;
}

namespace control {

class JsonLineWriter;

enum class AdapterResult {
  continue_running,
  shutdown,
};

class ControlInputAdapter {
public:
  /// @brief SidecarServiceとJsonLineWriterを参照してControlInputAdapterを構築する。
  ///
  /// Args:
  ///   service <serve::SidecarService&>: sidecar固有のstreamとprocess lifecycleを担当するservice。
  ///   writer <JsonLineWriter&>: responseとeventを書き出すJSON Lines writer。
  ///
  /// Return:
  ///   <ControlInputAdapter>: headless mapperとdispatcherを保持するadapter。
  ControlInputAdapter(serve::SidecarService& service, JsonLineWriter& writer);

  /// @brief ControlMessageを処理し、必要なresponseまたはeventを書き出す。
  ///
  /// Args:
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続するかshutdownするかの指示。
  AdapterResult handle(const ControlMessage& message);

private:
  enum class ScanCommandKind
  {
    start,
    status,
    stop,
  };

  enum class ProjectorCommandKind
  {
    list_monitors,
    configure_surface,
    open,
    close,
    generate,
    show,
    next,
    prev,
  };

  /// @brief camera resource commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///   close <bool>: close_cameraとしてstream停止を先行するか。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続する指示。
  AdapterResult handleCameraCommand(const std::string& id, const ControlMessage& message, bool close);

  /// @brief window resource commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///   close <bool>: close_windowとして処理するか。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続する指示。
  AdapterResult handleWindowCommand(const std::string& id, const ControlMessage& message, bool close);

  /// @brief projector commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///   kind <ProjectorCommandKind>: projector command種別。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続する指示。
  AdapterResult handleProjectorCommand(const std::string& id, const ControlMessage& message, ProjectorCommandKind kind);

  /// @brief scan commandをmapper/dispatcher経由で実行する。
  AdapterResult handleScanCommand(const std::string& id, const ControlMessage& message, ScanCommandKind kind);

  /// @brief scan dataset commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続する指示。
  AdapterResult handleScanDatasetCommand(const std::string& id, const ControlMessage& message);

  /// @brief decode commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続する指示。
  AdapterResult handleDecodeCommand(const std::string& id, const ControlMessage& message);

  /// @brief capture_frame系commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///   calibration <bool>: calibration用captureとしてpurposeとevent名を切り替えるか。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続するかshutdownするかの指示。
  AdapterResult handleCaptureFrameCommand(
      const std::string& id,
      const ControlMessage& message,
      bool calibration);

  /// @brief capture_stereo系commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///   calibration <bool>: calibration用captureとしてpurposeとevent名を切り替えるか。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続するかshutdownするかの指示。
  AdapterResult handleCaptureStereoCommand(
      const std::string& id,
      const ControlMessage& message,
      bool calibration);


  /// @brief calibration計算commandをmapper/dispatcher経由で実行する。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///   stereo <bool>: stereo calibration commandとして処理するか。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続するかshutdownするかの指示。
  AdapterResult handleCalibrationCommand(
      const std::string& id,
      const ControlMessage& message,
      bool stereo);

  /// @brief SidecarService由来の失敗をControlResponseへ変換して書き出す。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   result <const serve::SidecarResult&>: SidecarServiceから返された失敗結果。
  ///
  /// Return:
  ///   <void>: 戻り値なし。
  void writeServiceFailure(
      const std::string& id,
      const serve::SidecarResult& result);

  /// @brief command変換由来の失敗をControlResponseへ変換して書き出す。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   error <const common::CommandError&>: mapperから返されたerror。
  ///
  /// Return:
  ///   <void>: 戻り値なし。
  void writeHeadlessFailure(
      const std::string& id,
      const common::CommandError& error);

  /// service_ <serve::SidecarService&>: streamとprocess lifecycleを担当するservice。
  serve::SidecarService& service_;

  /// writer_ <JsonLineWriter&>: responseとeventを書き出すJSON Lines writer。
  JsonLineWriter& writer_;

  /// headless_mapper_ <headless::HeadlessCommandMapper>: ControlMessageをdomain commandへ変換するmapper。
  headless::HeadlessCommandMapper headless_mapper_;

  /// command_executor_ <headless::HeadlessCommandExecutor>: 型付きdomain commandを実行するexecutor。
  headless::HeadlessCommandExecutor command_executor_;
};

} // namespace control
