# Photodiode / Camera–Projector実機確認

Photodiode eventはMCUからUSB serialへ `0\n`（black）または`1\n`（white）として送る。
PCはvalid line受信時のhost `steady_clock`をevent timestampとする。Camera timestampも現行の
`current_frame_sample_host_timestamp`であり、hardware timestamp精度は保証しない。

## 1. Photodiode connection

```bash
PHOTODIODE_DEVICE=/dev/ttyUSB0 \
./scripts/test_photodiode_connection.sh
```

## 2. Delay measurement

白い平面をCameraへ映し、Projector表示を観測できる状態で実行する。

```bash
PHOTODIODE_DEVICE=/dev/ttyUSB0 \
CAMERA_ID=0 \
./scripts/measure_photodiode_delay.sh
```

`mean`、`median`、`p95`、`p99`、`max`、`recommended_guard_ms`を出力する。
recommended guardは `ceil(p99 + safety_margin_ms)` であり、default marginは5ms。

## 3. Camera–Projector connection check

```bash
PHOTODIODE_DEVICE=/dev/ttyUSB0 \
SYNC_GUARD_MS=<recommended_guard_ms> \
CAMERA_ID=0 \
./scripts/check_camera_projector.sh
```

表示された `[STREAM]` URLでMJPEGを確認する。

## 4. Mono calibration

```bash
CAMERA_ID=0 \
CAMERA_ROLE=left \
BOARD_X=10 \
BOARD_Y=7 \
SQUARE_MM=<実測値> \
./scripts/calibration_session.sh
```

## 5. Camera–Projector observation capture

```bash
PHOTODIODE_DEVICE=/dev/ttyUSB0 \
SYNC_GUARD_MS=<recommended_guard_ms> \
CAMERA_ID=0 \
./scripts/capture_camera_projector_poses.sh
```

CameraとProjectorは固定し、既存27 poseのcheckerboardだけを移動する。

## 6. Camera–Projector calibration

保存したobservationへ既存`camera_projector_calibrate` commandを実行する。

Synthetic testはsoftware contractだけを検証する。USB latency、scheduler jitter、physical Camera/Projector timingは
上記実機手順で確認する。
