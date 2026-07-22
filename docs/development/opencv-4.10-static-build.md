# OpenCV 4.10 Static Build

このドキュメントでは、`ToothProjectTools`で使用するOpenCV 4.10.0のstatic buildと、プロジェクトのbuild方法を説明します。

## 前提

Debian系のdevcontainerを想定します。

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  git \
  pkg-config \
  curl \
  unzip \
  libgtk-3-dev
```

## OpenCVのbuild

リポジトリルートで実行します。

```bash
chmod +x scripts/build_opencv_4_10_static.sh
./scripts/build_opencv_4_10_static.sh
```

デフォルト値:

```text
OpenCV version: 4.10.0
install prefix: ~/.local/opencv/4.10.0-static
work directory: /tmp/opencv-4.10.0-work
```

環境変数で上書きできます。

```bash
OPENCV_VERSION=4.10.0 \
OPENCV_INSTALL_PREFIX="$HOME/.local/opencv/4.10.0-static" \
OPENCV_WORK_DIR=/tmp/opencv-4.10.0-work \
JOBS="$(nproc)" \
./scripts/build_opencv_4_10_static.sh
```

## OpenCVの確認

```bash
OPENCV_PREFIX="${OPENCV_INSTALL_PREFIX:-$HOME/.local/opencv/4.10.0-static}"

test -f \
  "$OPENCV_PREFIX/lib/cmake/opencv4/OpenCVConfig.cmake" &&
  echo "OK: OpenCVConfig.cmake"

test -f \
  "$OPENCV_PREFIX/lib/opencv4/3rdparty/libade.a" &&
  echo "OK: libade.a"
```

## ToothProjectToolsのbuild

```bash
cmake --preset release-opencv-4.10-static
cmake --build --preset release-opencv-4.10-static --parallel
```

CTest presetが定義されている場合:

```bash
ctest \
  --preset release-opencv-4.10-static \
  --output-on-failure
```

backendを確認します。

```bash
TOOTH_BACKEND="$PWD/build/release-opencv-4.10-static/src/serve/tooth-backend"

test -x "$TOOTH_BACKEND" &&
  echo "OK: $TOOTH_BACKEND"
```

## トラブルシューティング

### `OpenCVConfig.cmake`が見つからない

```bash
find "$HOME/.local/opencv" \
  -name OpenCVConfig.cmake \
  -print
```

CMake presetの`OpenCV_DIR`が、実際の配置先を参照していることを確認してください。

### `libade.a`が見つからない

OpenCVのstatic buildに必要な3rdparty targetがinstallされていない可能性があります。

```bash
find "$HOME/.local/opencv/4.10.0-static" \
  -name 'libade.a' \
  -print
```

### GTK backendが無効

`libgtk-3-dev`を導入した状態でOpenCVを再configureしてください。
