# Stereo Scan Quickstart

Camera CalibrationからPoint Cloud生成までをmenuで進める場合は、初心者向けの [Hardware Quick Start](../development/hardware-quickstart.md) から開始してください。

`stereo_scan` は、左右Cameraの準備からGray Code撮影、decode、PLY生成までを1 commandで開始するAPIです。
sidecarとは標準入出力のJSON Linesで通信します。

## Shell Scriptで一括実行

`scripts/stereo_scan.sh` はbackendの起動からPLY確認、shutdownまでを1 commandで行うPhotodiode実機runnerです。
高レベル `stereo_scan` Facadeは使用せず、`scripts/check_camera_projector.sh` と同じpublic JSONL commandを、Camera、Window、Projector、scan、decode、reconstructionのdependency順に呼び出します。Windowの作成・配置とProjector表示は、それぞれ既存の `open_window` とProjector commandへ委譲します。

基本実行:

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=6 \
MONITOR_INDEX=1 \
./scripts/stereo_scan.sh
```

Photodiode設定を明示する場合:

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=6 \
MONITOR_INDEX=1 \
SYNC_MODE=photodiode \
PHOTODIODE_DEVICE=/dev/ttyUSB0 \
GUARD_MS=95 \
./scripts/stereo_scan.sh
```

出力先を指定する場合:

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=6 \
MONITOR_INDEX=1 \
OUTPUT_DIR=/workspace/data/scans/test \
PLY_FILE=/workspace/data/scans/test/result.ply \
./scripts/stereo_scan.sh
```

`MONITOR_INDEX` は必須で、`SYNC_MODE` は `photodiode` のみを受け付けます。`BIN`、`CALIBRATION_FILE`、`PHOTODIODE_BAUD`、`SYNC_TIMEOUT_MS`、`GUARD_MS`、`SCAN_ID`、`MJPEG_HOST`、`MJPEG_PORT` もenvironment variableで設定できます。`OUTPUT_DIR` のdefaultは `data/scans/<scan_id>`、`PLY_FILE` のdefaultは `<output_dir>/cloud.ply` です。その下に `scan/` と `decode/` を作成します。

`DEBUG_TIMING=1` でpatternごとの投影・Photodiode・Camera・Queue待機時間を表示します。`GUARD_MS=0` は性能診断専用で、本番Scanには推奨しません。
任意の `DISPLAY_WIDTH` / `DISPLAY_HEIGHT` と `CODE_WIDTH` / `CODE_HEIGHT` は、それぞれ必ずペアで指定します。`DECODE_THRESHOLD` と `MAX_EPIPOLAR_ERROR_PX` も必要な場合だけoverrideできます。

Photodiode deviceの存在・read/write permissionを検証しますが、Shell Script自身はserial deviceをopenしません。deviceは `scan_start` / `ScanService`だけが所有します。失敗時のscan artifactとbackend logは残り、stderrのpathが表示されます。正常時にも調査用work directoryを残すには `KEEP_WORK_DIR=1` を指定します。

Camera、stream、Projector、patternの準備が完了すると、active patternから32px離れた位置へ赤い96x96 markerを表示し、`[READY] SPACE: scan Q: quit` と表示して待機します。SPACEごとに固有のscan directoryでscan、decode、reconstructionを実行し、完了・失敗後は赤locatorへ戻ります。scan、decode、reconstructionの失敗はそのrunだけの失敗としてartifactを残し、`[READY] SPACE: retry Q: quit` から新しいscanを開始できます。Camera、stream、Window、Projectorは再作成せず、QまたはCtrl+Cで初めてsession resourceをcleanupします。

`OUTPUT_DIR=/workspace/data/scans/test2` の場合、各runは次のように分離されます。`PLY_FILE` を指定した場合も、そのbasenameを各run directory内で使用します。

```text
/workspace/data/scans/test2/
├── scan_20260929_061559_275192864/
│   ├── scan/
│   ├── decode/
│   └── result.ply
└── scan_20260929_061615_903441270/
    ├── scan/
    ├── decode/
    └── result.ply
```

## 1. Backendを起動

build済みbackendをserve modeで起動します。

```bash
build/release-opencv-4.10-static/src/serve/tooth-backend serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

stdoutに `ready` eventが出てからrequestを送信してください。

## 2. Calibrationを準備

事前にleft/right mono calibrationとstereo calibrationを完了し、次のfileを生成してください。

```text
data/calib/stereo.yml
```

手順は[Calibration command](./commands/calibration.md)を参照してください。

## 3. Scanを開始

最小requestは次の1行です。

```json
{"id":"scan-001","cmd":"stereo_scan","monitor_index":1}
```

主なdefault値:

- left camera: `0`
- right camera: `2`
- sync mode: `delay`
- delay: `100 ms`
- calibration: `data/calib/stereo.yml`
- Gray Code: `480x270`

`monitor_index` は0-basedです。`0` が1台目、`1` が2台目のmonitorです。接続monitorは
`{"id":"monitors","cmd":"list_monitors"}` で確認できます。

開始が受理されると、Camera previewのURLと出力予定pathがresponseで返ります。`scan_id` をrequestで省略した場合は自動生成されます。次の値は例です。

```json
{"id":"scan-001","ok":true,"scan_id":"scan_1790000000000000","left_stream_url":"http://127.0.0.1:39010/left.mjpg","right_stream_url":"http://127.0.0.1:39010/right.mjpg","output_dir":"data/scans/scan_1790000000000000","ply_file":"data/scans/scan_1790000000000000/cloud.ply"}
```

## 4. Progress event

scanは非同期で進み、stdoutへ次の順序でeventが流れます。

```text
stereo_scan_started
↓
stereo_scan_scanning
↓
stereo_scan_decoding
↓
stereo_scan_reconstructing
↓
stereo_scan_completed
```

GUIでは、これらを `Scanning...`、`Decoding...`、`Reconstructing...`、`Complete` などの表示に対応させられます。

## 5. 結果を取得

完了時は `stereo_scan_completed` eventを確認します。特に `left_stream_url`、`right_stream_url`、`ply_file` を利用します。

```json
{"event":"stereo_scan_completed","scan_id":"scan_1790000000000000","left_stream_url":"http://127.0.0.1:39010/left.mjpg","right_stream_url":"http://127.0.0.1:39010/right.mjpg","ply_file":"data/scans/scan_1790000000000000/cloud.ply","point_count":"182493"}
```

標準の出力構造:

```text
data/scans/<scan_id>/
├── scan/
├── decode/
└── cloud.ply
```

## Delay modeを明示する場合

Photodiodeを使用しない通常モードです。各pattern表示後、`delay_ms` を基準として左右画像を取得します。

```json
{"id":"scan-001","cmd":"stereo_scan","monitor_index":1,"sync_mode":"delay","delay_ms":100}
```

## Photodiodeを使う場合

Photodiodeの光学変化を基準に同期します。

```json
{"id":"scan-002","cmd":"stereo_scan","monitor_index":1,"sync_mode":"photodiode","photodiode_device":"/dev/ttyUSB0","guard_ms":99}
```

## 終了

`stereo_scan` 完了後もleft/right CameraとMJPEG streamは維持されます。不要になったら各roleのstreamを停止してCameraをcloseしてください。

```json
{"id":"stop-left","cmd":"stop_stream","role":"left"}
{"id":"close-left","cmd":"close_camera","role":"left"}
{"id":"stop-right","cmd":"stop_stream","role":"right"}
{"id":"close-right","cmd":"close_camera","role":"right"}
```

## 詳細仕様

- [stereo_scan command](./commands/stereo-scan.md)
- [Calibration](./commands/calibration.md)
- [Scan](./commands/scan.md)
