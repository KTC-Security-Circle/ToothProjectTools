# スキャンcommand

## scan_start

### 役割

scan_start はscan processを開始する。
このcommandは非同期である。
returnは受付結果である。
実際の進捗、完了、失敗はeventで返る。

### args(JSONL)

```json
{"id":"70","cmd":"scan_start","scan_id":"session_001","left_role":"left","right_role":"right","projector_role":"projector","output_dir":"./data/scan/session_001","photodiode_device":"/dev/ttyUSB0","photodiode_baud":115200,"sync_timeout_ms":1000,"sync_guard_ms":30}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `scan_start`。 |
| `scan_id` | 任意 | scan session ID。省略時は実装が生成する。 |
| `left_role` | 必須 | left camera role。 |
| `right_role` | 任意 | stereo時のright camera role。 |
| `projector_role` | 必須 | projector role。 |
| `output_dir` | 必須 | scan dataset出力directory。 |
| `photodiode_device` | 任意 | serial device。default `/dev/ttyUSB0`。 |
| `photodiode_baud` | 任意 | serial baud。default `115200`。 |
| `sync_timeout_ms` | 任意 | event/frame待機timeout。default `1000`。 |
| `sync_guard_ms` | 任意 | Photodiode event後のCamera選択guard。default `30`。 |

### return

```json
{"id":"70","ok":true,"scan_id":"session_001","status":"running","projector_role":"projector","left_role":"left","right_role":"right","output_dir":"./data/scan/session_001","pattern_count":"44","captured_count":"0","current_index":"-1"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | command受付が完了したか。 |
| `scan_id` | scan session ID。 |
| `status` | scan process状態。 |
| `projector_role` | 使用するprojector role。 |
| `left_role` | 使用するleft camera role。 |
| `right_role` | 使用するright camera role。 |
| `output_dir` | scan dataset出力directory。 |
| `pattern_count` | 全pattern数。 |
| `captured_count` | 取得済みpattern数。 |
| `current_index` | 現在処理中のpattern index。開始直後は `-1`。 |

### event

```json
{"event":"scan_started","scan_id":"session_001","projector_role":"projector","left_role":"left","right_role":"right","pattern_count":"44","output_dir":"./data/scan/session_001"}
{"event":"scan_frame_captured","scan_id":"session_001","pattern_index":"0","captured_count":"1","pattern_count":"44","left_path":"./data/scan/session_001/left/pattern_000.png","right_path":"./data/scan/session_001/right/pattern_000.png"}
{"event":"scan_completed","scan_id":"session_001","captured_count":"44","pattern_count":"44","output_dir":"./data/scan/session_001"}
{"event":"scan_failed","scan_id":"session_001","error_code":"capture_failed","error_message":"failed to capture frame","captured_count":"12","current_index":"12"}
```

### 読むArtifact

なし。

### 書くArtifact

scan dataset。`metadata.json` にはDecode用の `projector_width` / `projector_height` と `pattern_count` をGray Code論理解像度として保存し、monitor/display設定は `surface.surface_*` と `surface.display_*` に分けて保存する。

### 必要なruntime resource

open済みleft camera。
stereo時はopen済みright camera。
open済みprojector。
`0`/`1` eventを出力するPhotodiode serial device。
生成済みpattern。`scan_start` は生成済みpattern列を使用し、surface設定を使って暗黙に再生成しない。表示時だけdisplay regionへnearest-neighborで拡大する。
Scan worker。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `invalid_command` | role、output_dir、Photodiode設定が不正である。 |
| `invalid_scan_config` | scan設定が不正である。 |
| `invalid_output_path` | `output_dir` が不正、またはcapture出力pathが不正である。 |
| `camera_not_open` | camera roleがopenされていない。 |
| `projector_not_open` | projector roleがopenされていない。 |
| `pattern_not_generated` | patternが生成されていない。 |
| `scan_already_running` | scan processが実行中である。 |
| `directory_create_failed` | scan dataset directoryまたはcapture出力directoryを作成できない。 |
| `scan_start_failed` | metadata.jsonを書けず、scanを開始できない。 |
| `pattern_show_failed` | pattern表示に失敗した。 |
| `photodiode_device_not_found` | serial deviceが存在しない。 |
| `photodiode_permission_denied` | serial device権限がない。 |
| `photodiode_open_failed` | serial open/configurationに失敗した。 |
| `photodiode_timeout` | expected eventをtimeout内に受信できない。 |
| `photodiode_invalid_event` | `0`/`1`以外を受信した。 |
| `photodiode_marker_margin_unavailable` | active pattern外に32x32 markerを配置できない。 |
| `camera_frame_timeout` | event timestamp + guard以降のframeを取得できない。 |
| `capture_failed` | frame取得に失敗した。 |
| `empty_frame` | captureしたframeが空である。 |
| `file_write_failed` | capture画像を書けない。 |

## scan_status

### 役割

scan processの状態を返す。
scanを開始しない。
scanを停止しない。

### args(JSONL)

```json
{"id":"71","cmd":"scan_status"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `scan_status`。 |

### return

```json
{"id":"71","ok":true,"status":"running","scan_id":"session_001","captured_count":"12","pattern_count":"44","output_dir":"./data/scan/session_001"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | command処理が完了したか。 |
| `status` | `idle`、`running`、`stopping`、`completed`、`failed`。 |
| `scan_id` | scan session ID。 |
| `captured_count` | 取得済みpattern数。 |
| `pattern_count` | 全pattern数。 |
| `output_dir` | scan dataset出力directory。 |

### event

なし。

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

Scan worker。

### error code

| code | 条件 |
| --- | --- |
| `internal_error` | 状態取得で未処理例外が発生した。 |

## scan_stop

### 役割

実行中のscan processへ停止を要求する。
停止完了はeventで返る。

### args(JSONL)

```json
{"id":"72","cmd":"scan_stop"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `scan_stop`。 |

### return

```json
{"id":"72","ok":true,"status":"stopping","scan_id":"session_001"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | stop要求を受け付けたか。 |
| `status` | stop要求後の状態。 |
| `scan_id` | scan session ID。 |

### event

```json
{"event":"scan_stopped","scan_id":"session_001","output_dir":"./data/scan/session_001"}
```

### 読むArtifact

なし。

### 書くArtifact

停止時点までのscan dataset。`metadata.json` にはDecode用の `projector_width` / `projector_height` と `pattern_count` をGray Code論理解像度として保存し、monitor/display設定は `surface.surface_*` と `surface.display_*` に分けて保存する。

### 必要なruntime resource

Scan worker。

### error code

| code | 条件 |
| --- | --- |
| `scan_not_running` | scan processが実行中ではない。 |
| `internal_error` | 停止要求で未処理例外が発生した。 |
