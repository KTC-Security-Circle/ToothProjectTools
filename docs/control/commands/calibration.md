# キャリブレーションcommand

## mono_calibrate

### 役割

mono_calibrate はファイル処理commandである。
保存済み単眼calibration画像からmono calibration fileを生成する。
`apply_to_camera=true` の場合だけ、計算結果をopen済みcameraへ反映する。

### args(JSONL)

```json
{"id":"20","cmd":"mono_calibrate","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml","board_corners_x":10,"board_corners_y":7,"square_size_mm":"<MEASURED_VALUE>"}
```

```json
{"id":"20","cmd":"mono_calibrate","role":"left","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml","board_corners_x":10,"board_corners_y":7,"square_size_mm":"<MEASURED_VALUE>","apply_to_camera":true}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `mono_calibrate`。 |
| `image_folder` | 必須 | calibration画像directory。 |
| `output_file` | 必須 | 出力file。 |
| `board_corners_x` | 必須 | checkerboard横方向の内部corner数。正の整数。 |
| `board_corners_y` | 必須 | checkerboard縦方向の内部corner数。正の整数。 |
| `square_size_mm` | 必須 | 実測した1 squareの辺長(mm)。正数。暗黙defaultはない。 |
| `role` | 任意 | `apply_to_camera=true` の場合のみ必須。 |
| `apply_to_camera` | 任意 | trueの場合のみ、roleに対応するopen済みcameraへK/Dを反映する。省略時はfalse。 |

### return

```json
{"id":"20","ok":true,"role":"","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml","rms":"0.420000"}
```

| field | 説明 |
| --- | --- |
| `role` | camera role。未指定なら空文字列。 |
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
`RMS`, `image_width`, `image_height`, `board_corners_x`, `board_corners_y`, `square_size_mm`, `K`, `D`を保存する。このfileは`camera_projector_calibrate.camera_calibration_file`へそのまま指定できる。monoとCamera–Projector calibrationには同じcheckerboard実測値を指定する。

### 必要なruntime resource

なし。
`apply_to_camera=true` の場合のみopen済みcamera roleが必要である。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。`apply_to_camera=true` の場合は `role` も必須。 |
| `camera_not_open` | `apply_to_camera=true` でroleがopenされていない。 |
| `calibration_image_not_found` | calibration画像がない。 |
| `calibration_failed` | calibration計算に失敗した。 |
| `calibration_output_write_failed` | 結果fileを書けない。 |

## calib_detect_corners

### 役割

open済みcameraの現在frameでcheckerboardを検出し、corner描画済みpreviewを保存する。checkerboardが完全に検出できない場合もcommand自体は成功し、`found=false`を返す。

### args(JSONL)

```json
{"id":"preview-left","cmd":"calib_detect_corners","role":"left","output":"./data/calib/preview/left.png","board_corners_x":10,"board_corners_y":7,"square_size_mm":"<MEASURED_VALUE>"}
```

`id`, `cmd`, `role`, `output`, `board_corners_x`, `board_corners_y`, `square_size_mm`はすべて必須。board dimensionとsquare sizeは正数でなければならない。

### return

```json
{"id":"preview-left","ok":true,"role":"left","found":false,"corner_count":0,"expected_corner_count":70,"path":"./data/calib/preview/left.png"}
```

`found=true`の場合だけ`corner_count`は完全な内部corner数になる。camera未open、frame取得不能、preview出力不能はerrorである。

### 実機session

`scripts/calibration_session.sh`はbackend起動、camera open、MJPEG stream、preview、連番capture、mono calibration、shutdownを一つのsingle-key UIで実行する。`SQUARE_MM`には必ず実測値を指定する。

## stereo_calibrate

### 役割

stereo_calibrate はファイル処理commandである。
保存済み左右calibration画像とmono calibration fileからstereo calibration fileを生成する。
open済みcameraのK/Dではなく、`left_calibration_file` / `right_calibration_file` を主入力にする。

### args(JSONL)

```json
{"id":"21","cmd":"stereo_calibrate","left_dir":"./data/calib/stereo/left","right_dir":"./data/calib/stereo/right","left_calibration_file":"./data/calib/mono_left.yml","right_calibration_file":"./data/calib/mono_right.yml","output_file":"./data/calib/stereo.yml"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `stereo_calibrate`。 |
| `left_dir` | 必須 | left calibration画像directory。 |
| `right_dir` | 必須 | right calibration画像directory。 |
| `left_calibration_file` | 必須 | left mono calibration file。 |
| `right_calibration_file` | 必須 | right mono calibration file。 |
| `output_file` | 必須 | stereo calibration file出力path。 |
| `left_role` | 任意 | `apply_to_camera=true` の場合のみ必須。 |
| `right_role` | 任意 | `apply_to_camera=true` の場合のみ必須。 |
| `apply_to_camera` | 任意 | trueの場合のみ、left/right roleのopen済みcameraを確認する。省略時はfalse。 |

`image_folder_left` / `image_folder_right`、`left_image_folder` / `right_image_folder` は `left_dir` / `right_dir` の互換aliasとして受け付ける。

### return

```json
{"id":"21","ok":true,"left_role":"","right_role":"","left_dir":"./data/calib/stereo/left","right_dir":"./data/calib/stereo/right","output_file":"./data/calib/stereo.yml","rms":"0.620000"}
```

| field | 説明 |
| --- | --- |
| `left_role` | left camera role。未指定なら空文字列。 |
| `right_role` | right camera role。未指定なら空文字列。 |
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
left mono calibration file。
right mono calibration file。

### 書くArtifact

stereo calibration file。

### 必要なruntime resource

なし。
`apply_to_camera=true` の場合のみopen済みleft/right camera roleが必要である。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。`apply_to_camera=true` の場合は `left_role` / `right_role` も必須。 |
| `camera_not_open` | `apply_to_camera=true` でroleがopenされていない。 |
| `invalid_command` | `left_dir` と `right_dir` が同じpathである、または左右roleが同じcameraを指す。 |
| `calibration_file_not_found` | mono calibration fileが存在しない。 |
| `calibration_file_invalid` | mono calibration fileを読めない、またはK/Dが空である。 |
| `calibration_image_not_found` | calibration画像がない。 |
| `stereo_image_pair_mismatch` | 左右画像数が一致しない。 |
| `stereo_calibration_failed` | stereo calibrationに失敗した。 |
| `file_write_failed` | 結果fileを書けない。 |

## camera_projector_calibrate

保存済みのscan/decode artifactと、その前後に撮影したcheckerboard画像からCamera–Projector calibration ymlを生成するoffline commandである。live Camera/Projector resourceは使用しない。

```json
{"id":"cp-calib-001","cmd":"camera_projector_calibrate","observations_dir":"./data/calib/camera_projector/observations","camera_calibration_file":"./data/calib/mono_left.yml","output_file":"./data/calib/camera_projector.yml","board_corners_x":10,"board_corners_y":7,"square_size_mm":"<MEASURED_VALUE>","max_mean_displacement_px":"<EXPLICIT_VALUE>","max_corner_displacement_px":"<EXPLICIT_VALUE>","overwrite":false}
```

上例のplaceholderは実行時にはJSON numberへ置換する。`board_corners_x`、`board_corners_y`、`square_size_mm`、2つのmovement thresholdは必須である。board寸法は正で合計10 corner以上、`square_size_mm`は正、thresholdは0以上でなければならない。実測していないsquare寸法を仮定してはならない。`overwrite`の省略値はfalseである。

### Observation dataset

```text
observations_dir/
  pose_001/
    reference_before.png
    reference_after.png
    scan/metadata.json
    scan/left/pattern_000.png ...
    decode/metadata.json
    decode/left/projector_x.yml
    decode/left/projector_y.yml
    decode/left/valid_mask.png
  pose_002/
    ...
```

直下の非hidden directoryを名前の辞書順で処理する。各poseのscan/decode metadataのlogical projector resolutionとpattern rectangleは一致し、全poseでも同一でなければならない。decode mapとmaskおよび全reference画像は同じcamera image sizeを持つ。mono calibrationに画像寸法が保存されている場合も一致が必要である。現在のsolverはlogical projector `480x270`だけを受理する。

1 poseは、board/camera/projectorを固定し、`reference_before`をcaptureし、Gray Code scanをcaptureし、`reference_after`をcaptureし、`decode_patterns`を実行した後に上記directoryへ揃える。beforeからafterまでcheckerboard、camera、projector、lens、focus、zoomを動かしてはならない。near/middle/far × 9 orientationは推奨planであり、APIがexactly 27 posesを要求するわけではない。solverのminimumはvalid 3 posesで、invalid poseはrejectして残りを使用する。

### return

成功時は`output_file`、`total_pose_count`、`accepted_pose_count`、`rejected_pose_count`、`projector_rms`、`stereo_rms`、`projector_width`、`projector_height`を返す。

### Output contract

YMLには`mode`、board dimensions、`square_size_mm`、camera/projector dimensions、pattern rectangle、`camera_K`、`camera_D`、`projector_K`、`projector_D`、`R_camera_to_projector`、`T_camera_to_projector`、両RMSを保存する。Projector座標はdisplay physical pixelではなくlogical projector pixelである。

変換方向は次のとおりである。

```text
X_projector = R_camera_to_projector * X_camera + T_camera_to_projector
```

object pointを`square_size_mm`で構築するため、`T_camera_to_projector`の単位はmmである。

### error code

`camera_projector_invalid_config`、`camera_projector_observations_invalid`、`camera_projector_mono_load_failed`、`camera_projector_artifact_load_failed`、`camera_projector_inconsistent_surface`、`camera_projector_insufficient_poses`、`camera_projector_solve_failed`、`camera_projector_output_exists`、`camera_projector_output_write_failed`を区別する。
