#pragma once

#include "control/control_message.hpp"
#include "headless/headless_command_mapper.hpp"
#include "headless/headless_dispatcher.hpp"

namespace service {
class SidecarService;
struct SidecarResult;
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
  ///   service <service::SidecarService&>: sidecar lifecycle処理とrole bindingを担当するservice。
  ///   writer <JsonLineWriter&>: responseとeventを書き出すJSON Lines writer。
  ///
  /// Return:
  ///   <ControlInputAdapter>: headless mapperとdispatcherを保持するadapter。
  ControlInputAdapter(service::SidecarService& service, JsonLineWriter& writer);

  /// @brief ControlMessageを処理し、必要なresponseまたはeventを書き出す。
  ///
  /// Args:
  ///   message <const ControlMessage&>: JSON Linesからparseされたcontrol message。
  ///
  /// Return:
  ///   <AdapterResult>: serve loopを継続するかshutdownするかの指示。
  AdapterResult handle(const ControlMessage& message);

private:
  /// @brief SidecarService由来の失敗をControlResponseへ変換して書き出す。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   result <const service::SidecarResult&>: SidecarServiceから返された失敗結果。
  ///
  /// Return:
  ///   <void>: 戻り値なし。
  void writeServiceFailure(
      const std::string& id,
      const service::SidecarResult& result);

  /// @brief HeadlessDispatcher由来の失敗をControlResponseへ変換して書き出す。
  ///
  /// Args:
  ///   id <const std::string&>: responseへ設定するrequest id。
  ///   error <const headless::HeadlessCommandError&>: headless mapperまたはdispatcherから返されたerror。
  ///
  /// Return:
  ///   <void>: 戻り値なし。
  void writeHeadlessFailure(
      const std::string& id,
      const headless::HeadlessCommandError& error);

  /// service_ <service::SidecarService&>: lifecycle commandとrole bindingを担当するservice。
  service::SidecarService& service_;

  /// writer_ <JsonLineWriter&>: responseとeventを書き出すJSON Lines writer。
  JsonLineWriter& writer_;

  /// headless_mapper_ <headless::HeadlessCommandMapper>: ControlMessageをdomain commandへ変換するmapper。
  headless::HeadlessCommandMapper headless_mapper_;

  /// headless_dispatcher_ <headless::HeadlessDispatcher>: GUI非依存domain commandを実行するdispatcher。
  headless::HeadlessDispatcher headless_dispatcher_;
};

} // namespace control
