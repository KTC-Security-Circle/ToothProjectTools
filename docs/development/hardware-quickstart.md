# Hardware Quick Start

`scripts/hardware_quickstart.sh` は、Camera calibrationからStructured Light Scan、Gray Code decode、Point Cloud reconstructionまでをmenuから実行する初心者向けUIです。処理本体は既存のCalibration、Photodiode、Stereo Scan scriptを呼び出します。

## Recommended Workflow

```text
Build
  ↓
Camera ID確認
  ↓
Left Mono Calibration
  ↓
Right Mono Calibration
  ↓
Stereo Calibration
  ↓
Photodiode Connection Check
  ↓
Photodiode Locator + Delay Measurement
  ↓
recommended_guard_ms取得
  ↓
Stereo Scan
  ↓
Gray Code Decode
  ↓
Point Cloud Reconstruction
  ↓
cloud.ply
```

Calibration開始後はCameraの位置、角度、focus、zoom、resolutionを変更しないでください。動かすのはCheckerboardだけです。

## 事前確認

projectをbuildします。

```bash
cmake --preset release-opencv-4.10-static
cmake --build build/release-opencv-4.10-static -j"$(nproc)"
```

Camera IDを確認します。

```bash
v4l2-ctl --list-devices
```

Photodiode deviceを確認します。

```bash
ls -l /dev/ttyUSB*
```

## 起動

通常、最初に実行するcommandはこれだけです。

```bash
LEFT_CAMERA=4 \
RIGHT_CAMERA=6 \
MONITOR_INDEX=1 \
SQUARE_MM=0.5 \
./scripts/hardware_quickstart.sh
```

`4`、`6`、`1`、`0.5` は例であり、全環境共通ではありません。Camera IDは `v4l2-ctl`、monitor indexは接続構成、`SQUARE_MM` は印刷後に実測したCheckerboard 1 squareの辺長に合わせてください。`SQUARE_MM` を省略すると起動時に入力を求めます。

主な環境変数:

| 変数 | default | 意味 |
| --- | --- | --- |
| `LEFT_CAMERA` | `0` | Left Camera ID |
| `RIGHT_CAMERA` | `2` | Right Camera ID |
| `MONITOR_INDEX` | 未指定 | Projector monitor index |
| `BOARD_X` / `BOARD_Y` | `10` / `7` | Checkerboard内部corner数 |
| `SQUARE_MM` | なし | 実測したsquare size |
| `PHOTODIODE_DEVICE` | `/dev/ttyUSB0` | Photodiode serial device |
| `PHOTODIODE_BAUD` | `115200` | baud rate |
| `GUARD_MS` | `95` | scan guard |
| `DEBUG_TIMING` | `0` | `1`でpattern別の性能計測を表示 |

`GUARD_MS=0` は性能診断用であり、Photodiode同期後のCamera安定待ちを無効にするため、本番Scanには推奨しません。
| `OUT_DIR` | `data/calib` | calibration出力先 |

Projector操作を選ぶまで `MONITOR_INDEX` が未指定なら、その時点で入力できます。Delay Measurement後に取得した `recommended_guard_ms` は、確認後、そのQuick Start sessionのStereo Scanへ引き継げます。

## キー操作

### Hardware Quick Start

| Key | 操作 |
| --- | --- |
| `1` | Left Mono Calibration |
| `2` | Right Mono Calibration |
| `3` | Stereo Calibration |
| `4` | Photodiode menu |
| `5` | Stereo Scan + Decode + Reconstruction |
| `6` | Status |
| `H` | Help |
| `Q` | 終了確認 |

### Mono session

| Key | 操作 |
| --- | --- |
| `p` | Corner preview |
| `SPACE` | frame capture |
| `m` | Mono calibration |
| `i` | image count |
| `q` | sessionを終了してmain menuへ戻る |

### Stereo session

| Key | 操作 |
| --- | --- |
| `p` | Left/Right corner preview |
| `SPACE` | Stereo pair capture |
| `i` | captured pair count |
| `m` | Left/Right Mono calibration |
| `s` | Stereo calibration |
| `a` | Mono + Stereo calibration |
| `q` | sessionを終了してmain menuへ戻る |

### Stereo Scan

| Key | 操作 |
| --- | --- |
| `SPACE` | Scan + Decode + Reconstructionを1回実行 |
| `Q` / `q` | scan sessionを終了してmain menuへ戻る |

## 各menuの役割

Mono CalibrationではCamera 1台でCheckerboardを複数姿勢から撮影し、`mono_left.yml` または `mono_right.yml` を作成します。

Stereo Calibrationでは同じCheckerboardを左右Cameraへ同時に映し、pairを保存します。左右Mono YAMLがない場合、Quick Startは開始せず、不足fileを表示します。

Photodiode Connection CheckはserialのBLACK/WHITE eventを確認します。Locator + Delay MeasurementはProjectorへ `1668x1080 FULL WHITE + 96x96 RED locator` を表示し、C++ runtimeでCameraとの同期遅延を測定します。

Stereo Scanは既存のinteractive sessionを開きます。SPACEごとにGray Code capture、decode、stereo reconstruction、PLY保存まで進みます。

## Artifacts

Calibrationのdefault出力:

```text
data/calib/
├── mono_left/
├── mono_right/
├── mono_left.yml
├── mono_right.yml
├── stereo/
│   ├── left/
│   └── right/
└── stereo.yml
```

preview画像は `data/calib/preview/` に保存されます。

Stereo Scanのdefault出力はsessionと各scan runで分かれます。

```text
data/scans/<session>/
└── scan_<timestamp>/
    ├── scan/
    │   ├── left/
    │   └── right/
    ├── decode/
    └── cloud.ply
```

実際のpathはStereo Scan開始時と完了時にterminalへ表示されます。失敗したrunのartifactも診断用に保持されます。

## Troubleshooting

- Calibration fileが不足している場合は、menuに表示されたMonoまたはStereo Calibrationを先に実行します。
- Photodiodeが見つからない場合は `ls -l /dev/ttyUSB*` とread/write permissionを確認します。
- Camera IDが不明な場合は `v4l2-ctl --list-devices` を実行します。
- child sessionで `q` を押してもQuick Start全体は終了せず、main menuへ戻ります。
- Quick Start自体を終了する場合はmain menuで `Q`、続けて `Y` を押します。

詳細仕様は [Stereo Calibration Hardware Session](./stereo-calibration-session.md) と [Stereo Scan Quickstart](../control/stereo-scan-quickstart.md) を参照してください。


Webカメラの設定
```
# 露光を手動化して抑える
v4l2-ctl -d /dev/video4 --set-ctrl=auto_exposure=1
v4l2-ctl -d /dev/video4 --set-ctrl=exposure_time_absolute=1

# オートフォーカスを切ってピントを固定
v4l2-ctl -d /dev/video4 --set-ctrl=focus_automatic_continuous=0
v4l2-ctl -d /dev/video4 --set-ctrl=focus_absolute=50

# ズーム
v4l2-ctl -d /dev/video4 --set-ctrl=zoom_absolute=200

# 露光を手動化して抑える
v4l2-ctl -d /dev/video6 --set-ctrl=auto_exposure=1
v4l2-ctl -d /dev/video6 --set-ctrl=exposure_time_absolute=1

# オートフォーカスを切ってピントを固定
v4l2-ctl -d /dev/video6 --set-ctrl=focus_automatic_continuous=0
v4l2-ctl -d /dev/video6 --set-ctrl=focus_absolute=50

# ズーム
v4l2-ctl -d /dev/video6 --set-ctrl=zoom_absolute=200
```
