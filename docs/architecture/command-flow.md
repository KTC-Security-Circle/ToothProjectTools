# コマンド処理の流れ

## 処理順序

```text
1. serve app が標準入力からJSON Linesを読む。
2. ControlInputAdapter がJSONをparseする。
3. Command Mapper がControl MessageをCommandへ変換する。
4. Dispatch がCommandをHandlerへ渡す。
5. Handler がServiceを呼ぶ。
6. Service が処理を行い、Service Resultを返す。
7. Result Mapper がreturn用のJSON Lines responseを作る。
8. serve app がstdoutへreturnを出力する。
```

## 現在の主な経路

```text
stdin JSON Lines
  -> control::JsonLineReader
  -> control::ControlMessage
  -> control::ControlInputAdapter
  -> headless::HeadlessCommandMapper
  -> cmd::Command
  -> headless::HeadlessDispatcher
  -> handler
  -> service
  -> command_result_mapper
  -> common::CommandResult
  -> control::ControlResponse
  -> stdout JSON Lines
```

## 直接処理するcommand

| command | 処理 |
| --- | --- |
| `ping` | ControlInputAdapter がreturnを作る。 |
| `shutdown` | ControlInputAdapter が終了要求を返す。 |
| `start_stream` | ControlInputAdapter が `SidecarService::startStream` を呼ぶ。 |
| `stop_stream` | ControlInputAdapter が `SidecarService::stopStream` を呼ぶ。 |

## 通知の流れ

```text
ScanService
  -> ScanEventQueue
  -> ServeApp main loop
  -> JsonLineWriter
  -> stdout JSON Lines
```

`scan_start` のreturnは受付結果である。
scanの進捗、完了、失敗はeventで出力する。
