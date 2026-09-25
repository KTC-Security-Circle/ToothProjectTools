# Photodiode / Camera–Projector実機確認

測定とproduction scanは、同じ `ProjectorService`、`SerialPhotodiodeTransport`、
`PhotodiodeSyncSource`、Camera worker、`FrameSample` ring bufferを使用する。MCU protocolは
`0\n`（black）/ `1\n`（white）のままである。

Timestampの意味は次のとおりで、どちらもhardware timestampではない。

- Camera timestamp: `VideoCapture::read()` 成功直後のhost `steady_clock`
- Photodiode timestamp: `SerialPhotodiodeTransport` がvalid lineをreadした際のhost `steady_clock`

従って `recommended_guard_ms` は、現在のhost-side production pipelineに対する実測guardである。

## 実機手順

1. Photodiodeを接続し、`PHOTODIODE_DEVICE=/dev/ttyUSB0 ./scripts/test_photodiode_connection.sh` で確認する。
2. `scripts/check_camera_projector.sh` でCamera–Projector表示を確認する。
3. 表示された96x96の赤いlocator中央へPhotodiodeを固定する。
4. `t` でBLACK/WHITE transitionを確認する。
5. C++ runtimeによる同期遅延測定を行う。
6. 出力された `recommended_guard_ms` を `SYNC_GUARD_MS` に設定する。
7. `scripts/calibration_session.sh` でMono calibrationを行う。
8. `scripts/capture_camera_projector_poses.sh` でCamera–Projector poseを取得する。
9. `camera_projector_calibrate` を実行する。

## C++同期遅延測定

```bash
PHOTODIODE_DEVICE=/dev/ttyUSB0 \
CAMERA_ID=4 \
MONITOR_INDEX=1 \
./scripts/measure_photodiode_delay.sh
```

scriptはbackendを起動し、monitor列挙、Camera/Window/Projector open、surface設定、pattern生成、
赤locator表示まで行う。Waylandでもmonitor indexを既存runtimeへ渡すためdesktop absolute座標は不要である。
locator（96x96、余白に応じて64/32へfallback）とproduction sync marker（32x32）は中心が一致し、
配置後にPhotodiodeを動かす必要はない。

ENTER後はBLACK/WHITE baselineをCamera workerの実frame進行で取得する。baseline差が
`minimum_contrast`（default 30）以上のpixelだけをmaskとし、各pixelのbaseline中間値と差の符号で
white/blackを分類する。maskの `required_ratio`（default 0.90）以上が新状態になった最初の
`FrameSample.timestamp` を `Tcam` とする。Photodiode eventのhost timestampを `Tpd` とし、
`delay = Tcam - Tpd` を60回の交互transitionで測る。表示command時刻からの遅延ではない。

結果の `recommended_guard_ms = ceil(p99_ms + safety_margin_ms)`（default margin 5ms）は、そのまま
`scan_start.sync_guard_ms` または `SYNC_GUARD_MS` に渡せる。transition明細はdefaultで
`data/photodiode_delay.csv` に保存される。CSV書き込み失敗は測定成功を取り消さずwarningとして返る。

```bash
SYNC_GUARD_MS=40 ./scripts/check_camera_projector.sh
```

CameraとProjectorはcalibration中固定する。`reference_before` / `reference_after` はFULL WHITEで撮影し、
同期markerはGray Code active area外に表示される。Synthetic testはsoftware contractを検証し、USB latency、
scheduler jitter、物理的なCamera/Projector timingは上記の実機測定で確認する。
