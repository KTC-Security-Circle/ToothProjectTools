# スキャンcommand

## scan_start

### 役割

scan_start はscan processを開始する。
このcommandは非同期である。
returnは受付結果である。
実際の進捗、完了、失敗はeventで返る。

### args(JSONL)

```json
{"id":"70","cmd":"scan_start","scan_id":"session_001","left_role":"left","right_role":"right","projector_role":"projector","output_dir":"./data/scan/session_001","settle_ms":120}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `scan_start`。 |
| `scan_id` | 任意 | scan session ID。省略時は実装が生成する。 |
| `left_role` | 必須 | left camera role。 |
| `right_role` | 必須 | right camera role。 |
| `projector_role` | 必須 | projector role。 |
| `output_dir` | 必須 | scan dataset出力directory。 |
| `settle_ms` | 任意 | pattern表示後に待つ時間。 |

### return

```json
{"id":"70","ok":true,"scan_id":"session_001","status":"running","output_dir":"./data/scan/session_001"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | command受付が完了したか。 |
| `scan_id` | scan session ID。 |
| `status` | scan process状態。 |
| `output_dir` | scan dataset出力directory。 |

### event

```json
{"event":"scan_started","scan_id":"session_001"}
{"event":"scan_frame_captured","scan_id":"session_001","index":0}
{"event":"scan_completed","scan_id":"session_001","output_dir":"./data/scan/session_001"}
{"event":"scan_failed","scan_id":"session_001","error":{"code":"capture_failed","message":"failed to capture frame"}}
```

### 読むArtifact

なし。

### 書くArtifact

scan dataset。

### 必要なruntime resource

open済みleft camera。
open済みright camera。
open済みprojector。
生成済みpattern。
Scan worker。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `invalid_command` | role、output_dir、settle_msが不正である。 |
| `camera_not_open` | camera roleがopenされていない。 |
| `projector_not_open` | projector roleがopenされていない。 |
| `patterns_not_generated` | patternが生成されていない。 |
| `scan_already_running` | scan processが実行中である。 |
| `capture_failed` | frame取得に失敗した。 |
| `scan_output_write_failed` | scan datasetを書けない。 |

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

停止時点までのscan dataset。

### 必要なruntime resource

Scan worker。

### error code

| code | 条件 |
| --- | --- |
| `scan_not_running` | scan processが実行中ではない。 |
| `internal_error` | 停止要求で未処理例外が発生した。 |
