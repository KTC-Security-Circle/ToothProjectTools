# カメラcommand

## open_camera

### 役割

camera deviceをopenする。
openしたcameraをroleへbindする。

### args(JSONL)

```json
{"id":"2","cmd":"open_camera","camera_id":0,"role":"left"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `open_camera`。 |
| `camera_id` | 必須 | OpenCVへ渡すdevice index。 |
| `role` | 必須 | camera role。 |

### return

```json
{"id":"2","ok":true}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | commandが成功したか。 |

### event

```json
{"event":"camera_opened","camera_id":0,"role":"left"}
```

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

camera device。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `invalid_command` | `camera_id` が負である。 |
| `camera_open_failed` | cameraをopenできない。 |
| `scan_resource_busy` | scanが対象roleを使用中である。 |

## close_camera

### 役割

roleにbindされたcameraをcloseする。
stream中の場合はstreamを止めてからcloseする。

### args(JSONL)

```json
{"id":"3","cmd":"close_camera","role":"left"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `close_camera`。 |
| `role` | 必須 | camera role。 |

### return

```json
{"id":"3","ok":true}
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

open済みcamera role。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `role` がない。 |
| `camera_not_open` | roleがopenされていない。 |
| `scan_resource_busy` | scanが対象roleを使用中である。 |
