# キャリブレーションcommand

## mono_calibrate

### 役割

mono_calibrate はファイル処理commandである。
保存済み単眼calibration画像からmono calibration fileを生成する。
現在の実装では `role` とopen済みcameraへの依存が残っている。
この依存は仕様上の必須条件ではない。

### args(JSONL)

```json
{"id":"20","cmd":"mono_calibrate","role":"left","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `mono_calibrate`。 |
| `role` | 現在は必須 | camera role。仕様上はlabelである。 |
| `image_folder` | 必須 | calibration画像directory。 |
| `output_file` | 任意 | 出力file。省略時は `./data/calib/<role>_mono.yml`。 |

### return

```json
{"id":"20","ok":true,"role":"left","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml","rms":"0.420000"}
```

| field | 説明 |
| --- | --- |
| `role` | camera role。 |
| `image_folder` | 入力directory。 |
| `output_file` | 出力file。 |
| `rms` | calibration RMS。 |

### event

```json
{"event":"mono_calibration_finished","role":"left","output_file":"./data/calib/mono_left.yml","rms":"0.420000"}
```

### 読むArtifact

calibration画像directory。

### 書くArtifact

mono calibration file。

### 必要なruntime resource

現在の実装ではopen済みcamera roleが必要である。
仕様上は不要である。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `camera_not_open` | roleがopenされていない。現在の実装で発生する。 |
| `calibration_image_not_found` | calibration画像がない。 |
| `calibration_failed` | calibration計算に失敗した。 |
| `calibration_output_write_failed` | 結果fileを書けない。 |

## stereo_calibrate

### 役割

stereo_calibrate はファイル処理commandである。
保存済み左右calibration画像からstereo calibration fileを生成する。
現在の実装では `left_role` / `right_role` とopen済みcameraへの依存が残っている。
この依存は仕様上の必須条件ではない。

### args(JSONL)

```json
{"id":"21","cmd":"stereo_calibrate","left_role":"left","right_role":"right","left_dir":"./data/calib/stereo/left","right_dir":"./data/calib/stereo/right","output_file":"./data/calib/stereo.yml"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `stereo_calibrate`。 |
| `left_role` | 現在は必須 | left camera role。仕様上はlabelである。 |
| `right_role` | 現在は必須 | right camera role。仕様上はlabelである。 |
| `left_dir` | 必須 | left calibration画像directory。 |
| `right_dir` | 必須 | right calibration画像directory。 |
| `output_file` | 必須 | stereo calibration file出力path。 |
| `left_calibration_file` | 未対応 | left mono calibration file。 |
| `right_calibration_file` | 未対応 | right mono calibration file。 |

### return

```json
{"id":"21","ok":true,"left_role":"left","right_role":"right","left_dir":"./data/calib/stereo/left","right_dir":"./data/calib/stereo/right","output_file":"./data/calib/stereo.yml","rms":"0.620000"}
```

| field | 説明 |
| --- | --- |
| `left_role` | left camera role。 |
| `right_role` | right camera role。 |
| `left_dir` | left入力directory。 |
| `right_dir` | right入力directory。 |
| `output_file` | 出力file。 |
| `rms` | calibration RMS。 |

### event

```json
{"event":"stereo_calibration_finished","left_role":"left","right_role":"right","output_file":"./data/calib/stereo.yml","rms":"0.620000"}
```

### 読むArtifact

left calibration画像directory。
right calibration画像directory。

### 書くArtifact

stereo calibration file。

### 必要なruntime resource

現在の実装ではopen済みleft/right camera roleが必要である。
仕様上は不要である。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `camera_not_open` | roleがopenされていない。現在の実装で発生する。 |
| `invalid_command` | 左右roleが同じcameraを指す。 |
| `invalid_command` | `left_dir` と `right_dir` が同じpathである。 |
| `calibration_image_count_mismatch` | 左右画像数が一致しない。 |
| `stereo_calibration_failed` | stereo calibrationに失敗した。 |
| `calibration_output_write_failed` | 結果fileを書けない。 |
