# 撮影command

## capture_frame

### 役割

open済みcamera roleから1枚撮影し、画像fileへ保存する。

### args(JSONL)

```json
{"id":"6","cmd":"capture_frame","role":"left","output":"./data/mono_left/001.png"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `capture_frame`。 |
| `role` | 必須 | camera role。 |
| `output` | 必須 | 保存先画像path。 |

### return

```json
{"id":"6","ok":true,"path":"./data/mono_left/001.png"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | commandが成功したか。 |
| `path` | 保存した画像path。 |

### event

```json
{"event":"frame_saved","role":"left","path":"./data/mono_left/001.png"}
```

### 読むArtifact

なし。

### 書くArtifact

画像file。

### 必要なruntime resource

open済みcamera role。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `camera_not_open` | roleがopenされていない。 |
| `invalid_output_path` | output pathが不正である。 |
| `directory_create_failed` | directory作成に失敗した。 |
| `empty_frame` | frameが空である。 |
| `file_write_failed` | 画像を書けない。 |
| `scan_resource_busy` | scanが実行中である。 |

## capture_stereo

### 役割

left/right camera roleから撮影し、左右画像fileへ保存する。

### args(JSONL)

```json
{"id":"7","cmd":"capture_stereo","left_role":"left","right_role":"right","left_output":"./data/stereo/left_001.png","right_output":"./data/stereo/right_001.png"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `capture_stereo`。 |
| `left_role` | 必須 | left camera role。 |
| `right_role` | 必須 | right camera role。 |
| `left_output` | 必須 | left画像path。 |
| `right_output` | 必須 | right画像path。 |

### return

```json
{"id":"7","ok":true,"left_path":"./data/stereo/left_001.png","right_path":"./data/stereo/right_001.png"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | commandが成功したか。 |
| `left_path` | left画像path。 |
| `right_path` | right画像path。 |

### event

```json
{"event":"stereo_frame_saved","left_role":"left","right_role":"right","left_path":"./data/stereo/left_001.png","right_path":"./data/stereo/right_001.png"}
```

### 読むArtifact

なし。

### 書くArtifact

left画像file。
right画像file。

### 必要なruntime resource

open済みleft camera role。
open済みright camera role。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `camera_not_open` | roleがopenされていない。 |
| `invalid_command` | 左右roleが同じcameraを指す。 |
| `invalid_command` | 左右出力pathが同じである。 |
| `scan_resource_busy` | scanが実行中である。 |

## calib_capture_frame

### 役割

calibration用の単眼画像を保存する。
calibration計算は行わない。

### args(JSONL)

```json
{"id":"8","cmd":"calib_capture_frame","role":"left","output":"./data/calib/mono_left/001.png"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `calib_capture_frame`。 |
| `role` | 必須 | camera role。 |
| `output` | 必須 | 保存先画像path。 |

### return

```json
{"id":"8","ok":true,"path":"./data/calib/mono_left/001.png","purpose":"calibration"}
```

| field | 説明 |
| --- | --- |
| `path` | 保存した画像path。 |
| `purpose` | `calibration`。 |

### event

```json
{"event":"calibration_frame_saved","role":"left","path":"./data/calib/mono_left/001.png"}
```

### 読むArtifact

なし。

### 書くArtifact

calibration画像file。

### 必要なruntime resource

open済みcamera role。

### error code

`capture_frame` と同じである。

## calib_capture_stereo

### 役割

calibration用の左右画像を保存する。
calibration計算は行わない。

### args(JSONL)

```json
{"id":"9","cmd":"calib_capture_stereo","left_role":"left","right_role":"right","left_output":"./data/calib/stereo/left_001.png","right_output":"./data/calib/stereo/right_001.png"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `calib_capture_stereo`。 |
| `left_role` | 必須 | left camera role。 |
| `right_role` | 必須 | right camera role。 |
| `left_output` | 必須 | left画像path。 |
| `right_output` | 必須 | right画像path。 |

### return

```json
{"id":"9","ok":true,"left_path":"./data/calib/stereo/left_001.png","right_path":"./data/calib/stereo/right_001.png","purpose":"calibration"}
```

| field | 説明 |
| --- | --- |
| `left_path` | left画像path。 |
| `right_path` | right画像path。 |
| `purpose` | `calibration`。 |

### event

```json
{"event":"calibration_stereo_frame_saved","left_role":"left","right_role":"right","left_path":"./data/calib/stereo/left_001.png","right_path":"./data/calib/stereo/right_001.png"}
```

### 読むArtifact

なし。

### 書くArtifact

calibration用left画像file。
calibration用right画像file。

### 必要なruntime resource

open済みleft camera role。
open済みright camera role。

### error code

`capture_stereo` と同じである。
