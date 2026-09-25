# stereo_scan

Stereo Camera + Projector Structured-Light scanの非同期Facadeである。既存のcamera、stream、window、projector、scan、decode、reconstruction serviceを順にオーケストレーションし、各低レベルcommandは引き続き直接利用できる。

## 最小request

```json
{"id":"scan","cmd":"stereo_scan","monitor_index":1}
```

defaultはcamera `0/2`、role `left/right`、projector/window role `projector`、Gray Code `480x270`、calibration `data/calib/stereo.yml`、delay `100 ms`、decode threshold `15`、epipolar error `2.0 px`。`output_dir` は衝突しにくい `data/scans/scan_<generated>`、PLYはその直下の `cloud.ply` になる。

初期応答はcamera/stream開始後に返り、`scan_id`、`left_stream_url`、`right_stream_url`、`output_dir`、予定 `ply_file` を含む。Calibration fileは毎scanで生成せず再利用し、K1/D1/K2/D2、R/T、image sizeを開始前に検証する。未存在は `stereo_calibration_file_not_found`、不正fileは `stereo_calibration_file_invalid`。

## Sync

Delay（default）:

```json
{"id":"scan","cmd":"stereo_scan","monitor_index":1,"sync_mode":"delay","delay_ms":100}
```

Serial/Photodiodeをopenせず、marker余白を要求しない。表示時刻 + delay以降の最初の左右frameを同じ基準時刻で選ぶ。

Photodiode:

```json
{"id":"scan-pd","cmd":"stereo_scan","monitor_index":1,"sync_mode":"photodiode","photodiode_device":"/dev/ttyUSB0","photodiode_baud":115200,"sync_timeout_ms":1000,"guard_ms":99}
```

実際の光学transition + guard以降のframeを左右共通時刻で選ぶ。`guard_ms` とDelayの `delay_ms` は別の意味である。

## Output contract

```text
data/scans/<scan_id>/
├── scan/metadata.json, left/, right/
├── decode/metadata.json, left/, right/
└── cloud.ply
```

明示した `output_dir` / `ply_file` はdefaultより優先される。成功後もcameraとstreamは利用を継続し、既存のstop/close commandで終了する。

## Events and errors

`stereo_scan_started` → `stereo_scan_scanning`（途中の `scan_frame_captured`）→ `stereo_scan_decoding` → `stereo_scan_reconstructing` → `stereo_scan_completed`。

completed eventはstream URL、`scan_dir`、`decode_dir`、`ply_file`、文字列の `point_count` を含む。失敗時は `stereo_scan_failed` に `scan_id`、`stage`（camera/stream/window/projector/scan/decode/reconstruction）、元の `error_code`、`error_message` を含める。初期camera/stream/calibration失敗は同期responseとして返り、開始済みresourceをcleanupする。
