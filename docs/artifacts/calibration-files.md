# キャリブレーションファイル

## mono calibration file

mono calibration file は単眼cameraの内部parameterを保存するfileである。

### 生成するcommand

- `mono_calibrate`

### 読むcommand

- `stereo_calibrate`
- GUI calibration

### 必須field

| field | 説明 |
| --- | --- |
| `K` | camera内部parameter。 |
| `D` | 歪み係数。 |
| `RMS` | calibration RMS。 |

### 任意field

| field | 説明 |
| --- | --- |
| `version` | file format version。 |
| `role` | camera role。 |
| `created_at` | 生成時刻。 |
| `source_image_folder` | 入力画像directory。 |

### file format

OpenCV FileStorage YAMLである。

## stereo calibration file

stereo calibration file はleft/right camera間の外部parameterを保存するfileである。

### 生成するcommand

- `stereo_calibrate`

### 読むcommand

- `reconstruct_point_cloud`
- GUI 3D復元

### 必須field

| field | 説明 |
| --- | --- |
| `K1` | left camera内部parameter。 |
| `D1` | left camera歪み係数。 |
| `K2` | right camera内部parameter。 |
| `D2` | right camera歪み係数。 |
| `R` | leftからrightへの回転。 |
| `T` | leftからrightへの並進。 |
| `Q` | 視差-深度変換行列。 |
| `RMS` | stereo calibration RMS。 |

### 任意field

| field | 説明 |
| --- | --- |
| `version` | file format version。 |
| `left_role` | left camera role。 |
| `right_role` | right camera role。 |
| `created_at` | 生成時刻。 |
| `left_dir` | left入力画像directory。 |
| `right_dir` | right入力画像directory。 |

### file format

OpenCV FileStorage YAMLである。
