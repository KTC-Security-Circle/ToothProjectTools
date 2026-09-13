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
| `K` | camera内部parameter。3x3、single-channel、CV_32FまたはCV_64F。読み込み時はCV_64Fへ正規化する。 |
| `D` | 歪み係数。1xNまたはNx1、single-channel、CV_32FまたはCV_64F。係数数は4、5、8、12、14を許可する。読み込み時はCV_64Fへ正規化する。 |
| `RMS` | calibration RMS。 |

### 任意field

| field | 説明 |
| --- | --- |
| `version` | file format version。新規stereo calibrationは`0.1.0`。 |
| `role` | camera role。 |
| `created_at` | 生成時刻。 |
| `source_image_folder` | 入力画像directory。 |
| `image_width` | 新規mono calibrationの画像幅。単位pixel。生成時は`image_height`とともに保存する。 |
| `image_height` | 新規mono calibrationの画像高さ。単位pixel。生成時は`image_width`とともに保存する。 |
| `board_corners_x` | 生成に使ったcheckerboardの横方向内部corner数。 |
| `board_corners_y` | 生成に使ったcheckerboardの縦方向内部corner数。 |
| `square_size_mm` | 生成に使ったsquareの実測サイズ(mm)。 |

### file format

OpenCV FileStorage YAMLである。`K` / `D` にNaNまたはInfが含まれるfileは無効である。新規生成fileはtemporary YAMLへ完全に書き込んでからdestinationへ切り替えるため、書き込み失敗で既存の成功済みfileを途中状態にしない。

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
| `version` | file format version。新規出力は`0.1.0`。 |
| `image_width` | calibration画像幅。単位pixel。 |
| `image_height` | calibration画像高さ。単位pixel。 |
| `left_role` | left camera role。 |
| `right_role` | right camera role。 |
| `created_at` | 生成時刻。 |
| `left_dir` | left入力画像directory。 |
| `right_dir` | right入力画像directory。 |
| `board_corners_x` | 生成に使ったcheckerboardの横方向内部corner数。 |
| `board_corners_y` | 生成に使ったcheckerboardの縦方向内部corner数。 |
| `square_size_mm` | 生成に使ったsquareの実測サイズ(mm)。 |

### file format

OpenCV FileStorage YAMLである。ReconstructionServiceは`Q`が存在する場合、4x4 single-channel floating-point matrixかつ有限値であることを確認する。legacy fileでは`Q`がない形式も読み込むが、新規stereo calibrationでは必ず保存する。
新規生成fileは出力先と同じdirectoryに一意なtemporary YAMLを作成し、書き込み完了後にrenameする。solverまたは書き込みに失敗した場合はtemporary fileを削除し、既存destinationを保持する。`R` と `T` は既存ReconstructionServiceと同じく、left camera座標系からright camera座標系への変換を表す。
