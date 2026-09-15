# 概念定義

## 制御要求（Control Message）

Control Message は、serve appが標準入力から受け取るJSON Lines形式の要求である。

例:

```json
{"id":"80","cmd":"scan_validate","input_dir":"./data/scan/session_001"}
```

Control Messageは外部向けの形式である。
C++内部のCommandとは別である。
Control Messageは実処理を直接呼び出さない。
対応する実装は `control::ControlMessage` である。

## 入力処理（ControlInputAdapter）

ControlInputAdapter は、Control Messageを受け取る。
`ping`、`shutdown`、`start_stream`、`stop_stream` はここで処理する。
多くのcommandはCommand Mapperへ渡す。
ControlInputAdapterはcameraやfile artifactの実処理をしない。
対応する実装は `control::ControlInputAdapter` である。

## 型付き命令（Command）

Command は、C++内部で扱う型付き命令である。
CommandはJSON文字列を持たない。
Commandは処理を実行しない。
対応する実装は `cmd::Command` である。

## command変換（Command Mapper）

Command Mapper は、Control MessageをCommandへ変換する。
必須fieldの確認も行う。
一部のcommandではcamera roleをcamera idへ解決する。
Command MapperはServiceを直接呼ばない。
対応する実装は `headless::HeadlessCommandMapper` である。

## command実行（Command Executor）

Command Executorは、Command variantを一度だけ型判定し、対応するServiceを直接呼ぶ。
scan中のresource競合もService呼び出し前にここで拒否する。
Command Executorはcamera captureやdecode algorithmを実装せず、JSON Linesも組み立てない。
対応する実装は `headless::HeadlessCommandExecutor` である。

## 実処理（Service）

Service は実処理を行う。
ServiceはJSON Linesを知らない。
ServiceはControl Messageを受け取らない。
対応する実装は `CameraService`、`ProjectorService`、`WindowService`、`ScanService`、`CaptureService`、`DecodeService` である。

Serviceの配置は技術的役割ではなく、所有するdomainに従う。

```text
src/video          Camera / CameraManager / CameraService
src/window         Window / WindowManager / MonitorService / WindowService
src/projector      ProjectorService / ProjectorResult
src/capture        CaptureService
src/calibration    Calibrator / calibration service / calibration file I/O
src/scan           ScanService / ScanEvent / scan dataset validation
src/decode         DecodeService / DecodeResult
src/reconstruction ReconstructionService / CameraProjectorService
src/stream         MJPEG / FramePublisher / StreamRegistry
src/serve          ServeApp / SidecarService composition
```

`SidecarService`はdomain serviceではなく、serve runtimeが利用する各依存の生成、所有、lifecycleを担当する。
namespaceもpackage ownershipに合わせ、`video`、`win`、`projector`、`scan`、`decode`、`calib`、`serve`を使用する。

## 結果（Service Result）

Service Result は、Serviceが返す結果である。
Service固有のfieldを持つ。
例は `ScanResult`、`DecodePatternsResult` である。

## command結果（Command Result）

Command Result は、Command Executorが返す共通結果である。
成功か失敗かを持つ。
対応する実装は `common::CommandResult` である。

## 結果変換（Result Mapper）

Result Mapper は、Service ResultをCommand Resultへ変換する。
Result Mapperは業務処理を持たない。
Result Mapperはfileを読まない。
対応する実装は `src/command_result_mapper/` である。

## 成果物（Artifact）

Artifact は、filesystemに保存する成果物である。
後続のファイル処理commandはArtifactを入力として読む。
例はscan dataset、decode result、calibration file、PLY fileである。
