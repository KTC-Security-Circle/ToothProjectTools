# Stereo calibration hardware session

`scripts/stereo_calibration_session.sh` は、小型checkerboardを左右cameraで同時撮影し、左右のMono calibrationからStereo calibrationまでを既存backend commandで実行するdriverである。

## Checkerboard

defaultは11×8 squares、すなわちOpenCVへ渡す内部corner数 `BOARD_X=10`、`BOARD_Y=7` である。`SQUARE_MM` にdefaultはない。印刷設定上の寸法ではなく、印刷後に実測した1 squareの辺長をmmで指定する。

```bash
SQUARE_MM=0.5 \
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
./scripts/stereo_calibration_session.sh
```

`LEFT_CAMERA=0`、`RIGHT_CAMERA=2`、`LEFT_ROLE=left`、`RIGHT_ROLE=right` がdefaultである。`BIN`、`MJPEG_PORT`、`OUT_DIR` もenvironment variableで変更できる。

## Quick workflow

1. checkerboardを左右両方に映す。
2. `p` で70個（10×7）のcorner検出を確認する。
3. `SPACE` で左右同時pairを保存する。片側でも未検出なら保存されない。
4. checkerboardの位置・角度・距離を変えて繰り返す。
5. `m` でleft、rightのMono calibrationを実行する。
6. `s` でStereo calibrationを実行する。`a` は手順5と6を連続実行する。
7. `data/calib/stereo.yml` を確認する。

画像は同じpair番号で `data/calib/stereo/left/pair_NNN.png` と `data/calib/stereo/right/pair_NNN.png` に保存される。再起動時は既存番号の続きから開始し、左右の番号集合が一致しなければ撮影を開始しない。previewはdataset外の `data/calib/preview/` に保存される。

出力は以下である。

```text
data/calib/
├── stereo/left/pair_NNN.png
├── stereo/right/pair_NNN.png
├── mono_left.yml
├── mono_right.yml
└── stereo.yml
```

左右cameraの位置、角度、zoom、focus、resolutionはcalibration中、およびcalibration後に同じcamera構成でscanするまで変更しない。checkerboardだけを動かす。

## Small checkerboard note

corner refinementは既存Mono/Stereo実装の `cornerSubPix` 11×11 pixel windowを使用する。0.5 mmはobject-space scaleであり、それだけではpixel-space windowを変更する根拠にならない。実機で `findChessboardCorners` 成功後のsubpixel refinementに再現性のある問題が確認された場合に、画像上のsquare sizeに基づくwindow設計を別途検討する。

Stereo calibrationは左右Mono YAMLに保存された `image_width`、`image_height`、`board_corners_x`、`board_corners_y`、`square_size_mm`、`K`、`D` を読み、左右の寸法を検証する。したがってStereo solverはconstructor defaultではなく、Mono calibrationと同じ10×7および実測square sizeを使用する。

## Stereo scan

生成した `data/calib/stereo.yml` は `stereo_scan` の `calibration_file` として使用する。

```json
{
  "id": "scan",
  "cmd": "stereo_scan",
  "calibration_file": "data/calib/stereo.yml"
}
```

実行手順は [Stereo Scan Quickstart](../control/stereo-scan-quickstart.md) を参照する。
