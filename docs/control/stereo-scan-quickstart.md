# Stereo Scan Quickstart

`stereo_scan` は、左右Cameraの準備からGray Code撮影、decode、PLY生成までを1 commandで開始するAPIです。
sidecarとは標準入出力のJSON Linesで通信します。

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
