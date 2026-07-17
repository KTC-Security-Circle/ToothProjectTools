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
| `camera_matrix` | camera内部parameter。 |
| `distortion_coefficients` | 歪み係数。 |
| `image_width` | calibration画像幅。 |
| `image_height` | calibration画像高さ。 |
| `rms` | calibration RMS。 |

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
| `left_camera_matrix` | left camera内部parameter。 |
| `left_distortion_coefficients` | left camera歪み係数。 |
| `right_camera_matrix` | right camera内部parameter。 |
| `right_distortion_coefficients` | right camera歪み係数。 |
| `R` | leftからrightへの回転。 |
| `T` | leftからrightへの並進。 |
| `E` | essential matrix。 |
| `F` | fundamental matrix。 |
| `rms` | stereo calibration RMS。 |

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
