# キャリブレーションcommand

## 実機session script

`scripts/calibration_session.sh` はtooth-backendを1度だけ起動し、Enter不要の1-key UIでpreview、撮影、mono/stereo calibrationを順番に実行する。

```bash
BIN=./build/release-opencv-4.10-static/src/serve/tooth-backend \
LEFT_CAMERA=0 RIGHT_CAMERA=2 OUT_DIR=./data/calib \
BOARD_X=10 BOARD_Y=7 SQUARE_MM=10.0 \
./scripts/calibration_session.sh
```

`p` で左右corner preview、`c` でstereo pair、`l` / `r` でmono画像を撮影する。`1` / `2` / `3` は左mono / 右mono / stereo calibration、`i` は枚数表示、`q` はshutdownである。Stereo撮影は直前の `p` が `both_found=true` の場合のみ許可され、撮影後は再度previewが必要になる。画像番号は既存fileの最大番号の次から再開する。

## BoardConfig

corner preview、mono calibration、stereo calibrationは共通の `board_corners_x`、`board_corners_y`、`square_size_mm` を使う。すべて省略可能で、既定値は 10×7 corner / 10 mm。物理checkerboardの実測値を `square_size_mm` に指定する。指定値はすべて正でなければならない。

## calib_detect_corners

open済みcameraの最新frameに対し、GUIと同じ `Calibrator::detectAndDraw()` でcorner検出とoverlay描画を行う。検出失敗は `ok=true, found=false` の正常結果で、overlayのないcamera frameも保存される。preview画像はcalibration datasetへ登録されない。

```json
{"id":"preview-left","cmd":"calib_detect_corners","role":"left","output":"./data/calib/preview/left.png","board_corners_x":10,"board_corners_y":7,"square_size_mm":10.0}
```

return: `role`, `found`, `corner_count`, `expected_corner_count`, `path`。cameraがopenされていない場合や出力できない場合はcommand errorになる。

## calib_detect_stereo_corners

左右の最新frameを1回のpreview operationで取得し、同じBoardConfigで個別にcorner検出する。

```json
{"id":"preview-stereo","cmd":"calib_detect_stereo_corners","left_role":"left","right_role":"right","left_output":"./data/calib/preview/left.png","right_output":"./data/calib/preview/right.png","board_corners_x":10,"board_corners_y":7,"square_size_mm":10.0}
```

return: `left_found`, `right_found`, `both_found`, `left_corner_count`, `right_corner_count`, `expected_corner_count`, `left_path`, `right_path`。

## mono_calibrate

### 役割

mono_calibrate はファイル処理commandである。
保存済み単眼calibration画像からmono calibration fileを生成する。
`apply_to_camera=true` の場合だけ、計算結果をopen済みcameraへ反映する。

### args(JSONL)

```json
{"id":"20","cmd":"mono_calibrate","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml"}
```

```json
{"id":"20","cmd":"mono_calibrate","role":"left","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml","apply_to_camera":true}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `mono_calibrate`。 |
| `image_folder` | 必須 | calibration画像directory。 |
| `output_file` | 必須 | 出力file。 |
| `role` | 任意 | `apply_to_camera=true` の場合のみ必須。 |
| `apply_to_camera` | 任意 | trueの場合のみ、roleに対応するopen済みcameraへK/Dを反映する。省略時はfalse。 |
| `board_corners_x` | 任意 | checkerboardの横方向内部corner数。 |
| `board_corners_y` | 任意 | checkerboardの縦方向内部corner数。 |
| `square_size_mm` | 任意 | squareの実測サイズ(mm)。 |

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
| `board_corners_x` | 任意 | checkerboardの横方向内部corner数。 |
| `board_corners_y` | 任意 | checkerboardの縦方向内部corner数。 |
| `square_size_mm` | 任意 | squareの実測サイズ(mm)。 |

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
