# ストリームcommand仕様

## start_stream

### 役割

open済みcamera roleのMJPEG streamを開始する。

### args(JSONL)

```json
{"id":"4","cmd":"start_stream","role":"left"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `start_stream`。 |
| `role` | 必須 | camera role。 |

### return

```json
{"id":"4","ok":true,"url":"http://127.0.0.1:39010/left.mjpg"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | commandが成功したか。 |
| `url` | MJPEG endpoint URL。 |

### event

```json
{"event":"stream_started","role":"left","url":"http://127.0.0.1:39010/left.mjpg"}
```

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

open済みcamera role。
MJPEG server。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `role` がない。 |
| `camera_not_open` | roleがopenされていない。 |
| `stream_start_failed` | publisher開始に失敗した。 |

## stop_stream

### 役割

camera roleのMJPEG streamを停止する。

### args(JSONL)

```json
{"id":"5","cmd":"stop_stream","role":"left"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `stop_stream`。 |
| `role` | 必須 | camera role。 |

### return

```json
{"id":"5","ok":true}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | commandが成功したか。 |

### event

なし。

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

stream publisher。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `role` がない。 |
