# ToothProjectTools

`ToothProjectTools`は、カメラとプロジェクターを使用したStructured Light Scan、ステレオキャリブレーション、GrayCode decode、3D点群再構成を行うC++20プロジェクトです。

現在は、JSONLによるheadless backendを中心に、次の処理を提供します。

- カメラのopen、close
- MJPEG stream
- Mono calibration
- Stereo calibration
- Structured Light scan画像の取得
- GrayCode patternのdecode
- Reconstruction入力の検証
- PLY point-cloudの生成

---

## 技術スタック

- **言語 / 標準**
  - C++20

- **ビルド**
  - CMake
  - Ninja
  - mold
  - CMake Presets

- **画像処理**
  - OpenCV 4.10.0 static build
  - opencv_contrib
  - `structured_light`
  - `phase_unwrapping`

- **ログ**
  - spdlog

- **制御**
  - stdin / stdout JSONL protocol
  - headless backend
  - MJPEG server

- **解析・テスト**
  - cppcheck
  - CTest
  - shell smoke tests

- **開発環境**
  - Arch Linux host
  - VS Code Dev Containers
  - Docker Compose
  - X11
  - V4L2
  - GPU device共有

---

## クイックスタート

初回セットアップ、Dev Containerのrebuild後、または別環境へ移行した場合は、次のドキュメントから開始してください。

- [Development Guide](./docs/development/README.md)

推奨する確認順は次のとおりです。

```text
Dev Container
    ↓
OpenCV 4.10 static build
    ↓
ToothProjectTools build
    ↓
CTest
    ↓
Reconstruction fixture test
    ↓
Camera・Stream smoke test
    ↓
Structured Light scan
```

### 1. Dev Containerを開く

VS Codeでリポジトリを開き、コマンドパレットから次を実行します。

```text
Dev Containers: Rebuild and Reopen in Container
```

コンテナ内の作業ディレクトリ:

```text
/workspace
```

### 2. OpenCV 4.10 static buildを作成する

```bash
./scripts/build_opencv_4_10_static.sh
```

詳細:

- [OpenCV 4.10 Static Build](./docs/development/opencv-4.10-static-build.md)

OpenCVはデフォルトで次へinstallされます。

```text
/home/vscode/.local/opencv/4.10.0-static
```

### 3. ToothProjectToolsをbuildする

```bash
cmake --preset release-opencv-4.10-static
```

```bash
cmake \
  --build \
  --preset release-opencv-4.10-static \
  --parallel
```

テスト:

```bash
ctest \
  --preset release-opencv-4.10-static \
  --output-on-failure
```

backend確認:

```bash
test -x \
  build/release-opencv-4.10-static/src/serve/tooth-backend &&
  echo "OK: tooth-backend"
```

### 4. Backendを起動する

```bash
build/release-opencv-4.10-static/src/serve/tooth-backend \
  serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

起動すると、stdoutへready eventが出力されます。

```json
{"event":"ready","version":"0.1.0"}
```

疎通確認:

```json
{"id":"ping-1","cmd":"ping"}
```

---

## Smoke test

### Reconstruction fixture test

実機を使用せず、次の一連の処理を確認します。

- Left mono calibration
- Right mono calibration
- Stereo calibration
- GrayCode decode
- Reconstruction validation
- PLY point-cloud生成

```bash
./scripts/check_reconstruction_fixture.sh \
  "$HOME/Downloads/tooth-reconstruction-fixture-v1.zip"
```

詳細:

- [Reconstruction Fixture](./docs/development/reconstruction-fixture.md)

### Camera・Stream smoke test

実機カメラを接続して実行します。

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
./scripts/check_camera_stream_scan.sh
```

確認対象:

- backend起動
- ping
- Left/Right camera open
- Left/Right MJPEG stream
- MJPEGデータ受信
- 単発capture
- Stereo capture
- stream stop
- camera close
- backend shutdown

詳細:

- [Camera・Stream・Scan Smoke Test](./docs/development/camera-stream-scan-smoke-test.md)

### Structured Light scan

プロジェクターでpatternを表示できる状態にして実行します。

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
PATTERN_COUNT=46 \
PROJECTOR_WIDTH=1920 \
PROJECTOR_HEIGHT=1080 \
./scripts/run_structured_light_scan_test.sh \
  "$HOME/tooth-scan-test"
```

生成されるscan dataset:

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

---

## JSONL control protocol

backendはstdinから1行1JSON形式のcommandを受け取り、stdoutへresponseまたはeventを返します。

例:

```json
{"id":"open-left","cmd":"open_camera","camera_id":0,"role":"left"}
```

```json
{"id":"start-left","cmd":"start_stream","role":"left"}
```

```json
{"id":"capture-left","cmd":"capture_frame","role":"left","output":"/tmp/left.png"}
```

```json
{"id":"shutdown-1","cmd":"shutdown"}
```

詳細:

- [JSONL Control Protocol](./docs/control/protocol.md)
- [Command Index](./docs/control/command-index.md)

---

## リポジトリ構成

```text
.
├── .devcontainer/
│   ├── devcontainer.json
│   ├── docker-compose.yml
│   └── Dockerfile
├── CMakeLists.txt
├── CMakePresets.json
├── Makefile
├── README.md
├── cppcheck.supp
├── docs/
│   ├── architecture/
│   │   ├── overview.md
│   │   ├── concepts.md
│   │   └── command-flow.md
│   ├── artifacts/
│   │   └── scan-dataset.md
│   ├── control/
│   │   ├── protocol.md
│   │   └── command-index.md
│   ├── development/
│   │   ├── README.md
│   │   ├── opencv-4.10-static-build.md
│   │   ├── reconstruction-fixture.md
│   │   └── camera-stream-scan-smoke-test.md
│   └── testing/
│       └── test-strategy.md
├── scripts/
│   ├── build_opencv_4_10_static.sh
│   ├── package_reconstruction_fixture.sh
│   ├── check_reconstruction_fixture.sh
│   ├── check_camera_stream_scan.sh
│   └── run_structured_light_scan_test.sh
├── src/
│   ├── app/
│   ├── calib/
│   ├── cmd/
│   ├── common/
│   ├── control/
│   ├── input/
│   ├── logger/
│   ├── runtime/
│   ├── service/
│   ├── serve/
│   ├── sl/
│   ├── stream/
│   ├── video/
│   └── window/
└── tests/
```

実際のディレクトリ構成に差異がある場合は、この一覧より実装を優先してください。

---

## ドキュメント

### 開発環境

- [Development Guide](./docs/development/README.md)
- [OpenCV 4.10 Static Build](./docs/development/opencv-4.10-static-build.md)
- [Reconstruction Fixture](./docs/development/reconstruction-fixture.md)
- [Camera・Stream・Scan Smoke Test](./docs/development/camera-stream-scan-smoke-test.md)

### Architecture

- [Architecture Overview](./docs/architecture/overview.md)
- [Architecture Concepts](./docs/architecture/concepts.md)
- [Command Flow](./docs/architecture/command-flow.md)

### Protocol・Artifacts

- [JSONL Control Protocol](./docs/control/protocol.md)
- [Command Index](./docs/control/command-index.md)
- [Scan Dataset Specification](./docs/artifacts/scan-dataset.md)

### Testing

- [Test Strategy](./docs/testing/test-strategy.md)

---

## Dev Container

Dev Containerでは、Docker Composeを通じて次を共有します。

- `/workspace`
- `/tmp/.X11-unix`
- `/dev/video*`
- `/dev/dri`
- ccache named volume
- OpenCV 4.10 static build named volume

カメラデバイス番号は環境ごとに異なります。

```bash
v4l2-ctl --list-devices
```

現在のvideo device:

```bash
ls -l /dev/video*
```

Dev Containerの初回作成やrebuild後の手順は、次を参照してください。

- [Development Guide](./docs/development/README.md)

---

## OpenCV named volume

OpenCV static buildはnamed volumeへ保持されます。

そのため、Dev Containerをrebuildしてもnamed volumeが残っていれば、通常はOpenCVを再buildする必要はありません。

確認:

```bash
test -f \
  "$HOME/.local/opencv/4.10.0-static/lib/cmake/opencv4/OpenCVConfig.cmake" &&
  echo "OK: OpenCV is installed"
```

named volumeを削除した場合や、OpenCVのbuild条件を変更した場合は再buildしてください。

```bash
./scripts/build_opencv_4_10_static.sh
```

---

## ログ

実行時ログレベルは`SPDLOG_LEVEL`で変更できます。

```bash
export SPDLOG_LEVEL="info,app=trace"
```

backend起動:

```bash
SPDLOG_LEVEL="info,app=trace" \
  build/release-opencv-4.10-static/src/serve/tooth-backend \
  serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

---

## 静的解析

cppcheck:

```bash
cmake --preset release-opencv-4.10-static
cmake --build --preset release-opencv-4.10-static --target cppcheck-project
```

target名が異なる場合は、次で確認してください。

```bash
cmake \
  --build \
  --preset release-opencv-4.10-static \
  --target help
```

---

## ライセンス

[MIT License](./LICENSE)