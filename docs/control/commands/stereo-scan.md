# stereo_scan

Stereo Camera + Projector Structured-Light scanの非同期Facadeである。既存のcamera、stream、window、projector、scan、decode、reconstruction serviceを順にオーケストレーションし、各低レベルcommandは引き続き直接利用できる。

## 最小request

```json
{"id":"scan","cmd":"stereo_scan","monitor_index":1}
```

defaultはcamera `0/2`、role `left/right`、projector/window role `projector`、Gray Code論理解像度 `480x270`、calibration `data/calib/stereo.yml`、delay `100 ms`、decode threshold `15`、epipolar error `2.0 px`。物理displayは選択したmonitorの実解像度全体を使用する。例えばmonitor 1が1920x1080なら、codeは480x270のままdisplayだけ1920x1080となり、既存nearest-neighbor表示を使う。物理解像度はhard-codeしない。`output_dir` は衝突しにくい `data/scans/scan_<generated>`、PLYはその直下の `cloud.ply` になる。

`stereo_scan` はOSやwindow managerを判定しない。`monitor_index` を `WindowService` へ渡し、`WindowPlacementService` がX11/niri等のbackendを選んで配置する。追加のplatform固有fieldは不要である。配置未対応sessionでは推測で別monitorへ置かず、window stageが `window_placement_unsupported` で失敗する。

初期応答はcamera/stream開始後に返り、`scan_id`、`left_stream_url`、`right_stream_url`、`output_dir`、予定 `ply_file` を含む。Calibration fileは毎scanで生成せず再利用し、K1/D1/K2/D2、R/T、image sizeを開始前に検証する。未存在は `stereo_calibration_file_not_found`、不正fileは `stereo_calibration_file_invalid`。

## Sync

Delay（default）:

```json
{"id":"scan","cmd":"stereo_scan","monitor_index":1,"sync_mode":"delay","delay_ms":100}
```

Serial/Photodiodeをopenせず、marker余白を要求しない。表示時刻 + delay以降の最初の左右frameを同じ基準時刻で選ぶ。

`display_width` / `display_height` を両方指定した場合はmonitor全体defaultより優先する。片方だけの指定は `missing_field` でrejectする。

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

明示した `output_dir` / `ply_file` はdefaultより優先される。

## Repeated scan and runtime resources

同一sidecar processで `stereo_scan` を連続実行できる。既存roleが同じphysical deviceならCameraと実行中stream URLを再利用し、再open/restartしない。異なるdeviceへbind済みなら既存resourceを変更せず `camera_role_device_conflict` を返す。途中失敗時は、そのrequestが新規作成したresourceだけをcleanupする。

`stereo_scan_completed` 後の状態:

```text
left camera:  open
right camera: open
left stream:  running
right stream: running
projector:    closed
window:       closed
PLY:          generated
```

preview streamは完了後も利用できる。終了時は既存の `stop_stream` と `close_camera` を使用する。Window/Projectorはscan完了直後、Decode/Reconstructionより前に解放され、次回scanで再作成される。

## Events and errors

`stereo_scan_started` → `stereo_scan_scanning`（途中の `scan_frame_captured`）→ `stereo_scan_decoding` → `stereo_scan_reconstructing` → `stereo_scan_completed`。

completed eventはstream URL、`scan_dir`、`decode_dir`、`ply_file`、文字列の `point_count` を含む。失敗時は `stereo_scan_failed` に `scan_id`、`stage`（calibration/camera/stream/window/projector/scan/decode/reconstruction）、元の `error_code`、`error_message` を含める。初期camera/stream/calibration失敗は同期responseとしても返り、このrequestが開始したresourceだけをcleanupする。
