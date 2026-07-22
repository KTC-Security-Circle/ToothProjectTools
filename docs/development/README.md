# Development Guide

このドキュメントは、`ToothProjectTools`の開発環境構築と動作確認の入口です。

初回セットアップ、Dev Containerのrebuild後、開発環境を別PCへ移行した場合は、このドキュメントから開始してください。

## ドキュメント一覧

| 目的 | ドキュメント |
| --- | --- |
| OpenCV 4.10.0のstatic build | [OpenCV 4.10 Static Build](./opencv-4.10-static-build.md) |
| CalibrationからPLY再構成までの確認 | [Reconstruction Fixture](./reconstruction-fixture.md) |
| Camera、MJPEG stream、scan撮影の確認 | [Camera・Stream・Scan Smoke Test](./camera-stream-scan-smoke-test.md) |

## 推奨確認順

```text
Dev Container
    ↓
OpenCV static build
    ↓
ToothProjectTools build
    ↓
CTest
    ↓
Reconstruction fixture
    ↓
Camera・Stream smoke test
    ↓
Structured Light scan
```

Reconstruction fixtureは実機を必要としません。

Camera・Stream・Scan smoke testは、カメラまたはプロジェクターを接続した環境で実行します。

## 初回セットアップ

### 1. Dev Containerを作成する

VS Codeでリポジトリを開き、コマンドパレットから次を実行します。

```text
Dev Containers: Rebuild and Reopen in Container
```

コンテナへ接続後、作業ディレクトリを確認します。

```bash
pwd
```

期待値:

```text
/workspace
```

### 2. OpenCV用ディレクトリの権限を確認する

```bash
OPENCV_PREFIX="$HOME/.local/opencv/4.10.0-static"

mkdir -p "$OPENCV_PREFIX"

test -w "$OPENCV_PREFIX" &&
  echo "OK: OpenCV install directory is writable"
```

書き込めない場合:

```bash
sudo chown -R \
  "$(id -u):$(id -g)" \
  "$HOME/.local/opencv"
```

### 3. OpenCVをbuildする

```bash
./scripts/build_opencv_4_10_static.sh
```

詳細:

- [OpenCV 4.10 Static Build](./opencv-4.10-static-build.md)

### 4. OpenCVの生成物を確認する

```bash
OPENCV_PREFIX="$HOME/.local/opencv/4.10.0-static"

test -f \
  "$OPENCV_PREFIX/lib/cmake/opencv4/OpenCVConfig.cmake" &&
  echo "OK: OpenCVConfig.cmake"

test -f \
  "$OPENCV_PREFIX/lib/opencv4/3rdparty/libade.a" &&
  echo "OK: libade.a"
```

`OpenCV_DIR`も確認します。

```bash
printf '%s\n' "$OpenCV_DIR"
```

期待値:

```text
/home/vscode/.local/opencv/4.10.0-static/lib/cmake/opencv4
```

### 5. CMake presetを確認する

```bash
cmake --version
cmake --list-presets
```

次のpresetが表示されることを確認します。

```text
debug
release
release-opencv-4.10-static
```

### 6. ToothProjectToolsをbuildする

```bash
cmake --preset release-opencv-4.10-static
```

```bash
cmake \
  --build \
  --preset release-opencv-4.10-static \
  --parallel
```

backendを確認します。

```bash
TOOTH_BACKEND="$PWD/build/release-opencv-4.10-static/src/serve/tooth-backend"

test -x "$TOOTH_BACKEND" &&
  echo "OK: $TOOTH_BACKEND"
```

### 7. CTestを実行する

```bash
ctest \
  --preset release-opencv-4.10-static \
  --output-on-failure
```

## Dev Containerのrebuild後

OpenCVのnamed volumeが残っていれば、通常はOpenCVを再buildする必要はありません。

次で確認します。

```bash
OPENCV_CONFIG="$HOME/.local/opencv/4.10.0-static/lib/cmake/opencv4/OpenCVConfig.cmake"

if [[ -f "$OPENCV_CONFIG" ]]; then
  echo "OK: existing OpenCV installation found"
else
  echo "OpenCV must be rebuilt"
fi
```

OpenCVが存在する場合は、ToothProjectToolsだけをbuildします。

```bash
cmake --preset release-opencv-4.10-static

cmake \
  --build \
  --preset release-opencv-4.10-static \
  --parallel
```

named volumeを削除した場合や、OpenCVのbuild設定を変更した場合は再buildします。

```bash
./scripts/build_opencv_4_10_static.sh
```

## Software smoke test

最初に、実機を必要としないreconstruction fixtureを実行します。

fixtureの取得方法とGoogle Driveリンクは次に記載されています。

- [Reconstruction Fixture](./reconstruction-fixture.md)

実行:

```bash
./scripts/check_reconstruction_fixture.sh \
  "$HOME/Downloads/tooth-reconstruction-fixture-v1.zip"
```

正常終了:

```text
PASS: reconstruction fixture
```

このテストでは次を確認します。

- Left mono calibration
- Right mono calibration
- Stereo calibration
- GrayCode decode
- Reconstruction validation
- PLY point-cloud生成
- PLY vertex数
- NaNおよびInfの有無

## Hardware smoke test

実機カメラを接続して実行します。

デバイスを確認します。

```bash
v4l2-ctl --list-devices
```

Camera・Stream smoke test:

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
./scripts/check_camera_stream_scan.sh
```

詳細:

- [Camera・Stream・Scan Smoke Test](./camera-stream-scan-smoke-test.md)

このテストでは次を確認します。

- backend起動
- ping
- Left/Right camera open
- Left/Right MJPEG stream
- MJPEGデータ受信
- 単発capture
- Stereo capture
- Camera close
- backend shutdown

## Structured Light scan

Camera・Stream smoke testが完了した後に実行します。

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
PATTERN_COUNT=46 \
PROJECTOR_WIDTH=1920 \
PROJECTOR_HEIGHT=1080 \
./scripts/run_structured_light_scan_test.sh \
  "$HOME/Downloads/tooth-scan-test"
```

出力:

```text
tooth-scan-test/
├── left/
│   ├── pattern_000.png
│   └── ...
├── right/
│   ├── pattern_000.png
│   └── ...
└── metadata.json
```

## 一括確認

OpenCVがすでにbuild済みの場合、次の順で確認できます。

```bash
bash -n scripts/*.sh

cmake --preset release-opencv-4.10-static

cmake \
  --build \
  --preset release-opencv-4.10-static \
  --parallel

ctest \
  --preset release-opencv-4.10-static \
  --output-on-failure
```

Reconstruction fixture:

```bash
./scripts/check_reconstruction_fixture.sh \
  "$HOME/Downloads/tooth-reconstruction-fixture-v1.zip"
```

Camera smoke test:

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
./scripts/check_camera_stream_scan.sh
```

## トラブルシューティング

### `No such preset`

```bash
cmake --list-presets
```

`release-opencv-4.10-static`が存在することを確認してください。

また、CMakeのバージョンを確認します。

```bash
cmake --version
```

`CMakePresets.json`のschema version 6を使用する場合、CMake 3.25以上が必要です。

### OpenCVのinstall先へ書き込めない

```bash
stat -c '%U:%G %a %n' \
  "$HOME/.local/opencv/4.10.0-static"
```

修正:

```bash
sudo mkdir -p \
  "$HOME/.local/opencv/4.10.0-static"

sudo chown -R \
  "$(id -u):$(id -g)" \
  "$HOME/.local/opencv"
```

### Backendが見つからない

```bash
find build \
  -type f \
  -name tooth-backend \
  -print
```

既定位置:

```text
build/release-opencv-4.10-static/src/serve/tooth-backend
```

### カメラが開けない

```bash
v4l2-ctl --list-devices
ls -l /dev/video*
id
```

Dev Containerへ対象デバイスが渡されていることと、ユーザーがvideo groupを利用できることを確認してください。

### 左右画像の解像度が一致しない

```bash
v4l2-ctl -d /dev/video0 --get-fmt-video
v4l2-ctl -d /dev/video2 --get-fmt-video
```

Stereo calibrationとscanには、左右で同じ画像サイズを使用してください。