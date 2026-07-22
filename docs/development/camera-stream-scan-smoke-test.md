# Camera・Stream・Scan Smoke Test

このドキュメントでは、実機カメラを使って次を確認します。

1. backend起動
2. Left/Right camera open
3. MJPEG stream start
4. stream endpointからのデータ受信
5. 単発capture
6. stereo capture
7. stream stop
8. camera close
9. Structured Light scan dataset作成
10. scan dataset validation

再構成fixtureの確認は別ドキュメントです。

- [Reconstruction fixture](./reconstruction-fixture.md)

## 前提

デフォルト:

```text
Left camera: 0
Right camera: 2
MJPEG host: 127.0.0.1
MJPEG port: 39010
backend: build/release-opencv-4.10-static/src/serve/tooth-backend
```

実際のデバイスを確認します。

```bash
v4l2-ctl --list-devices
```

## Camera・stream smoke test

```bash
chmod +x scripts/check_camera_stream_scan.sh
```

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
./scripts/check_camera_stream_scan.sh
```

backendを変更する場合:

```bash
TOOTH_BACKEND=/path/to/tooth-backend \
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
./scripts/check_camera_stream_scan.sh
```

正常終了:

```text
PASS: camera and stream smoke test
```

スクリプトは次を確認します。

- backendのready event
- ping
- 左右カメラopen
- 左右stream start
- MJPEG endpointからデータを受信できる
- 左右単発capture
- stereo capture
- 生成画像が空でない
- 左右画像サイズが一致する
- stream stop
- camera close
- shutdown

一時ファイルを残す場合:

```bash
KEEP_WORK_DIR=1 \
./scripts/check_camera_stream_scan.sh
```

## Structured Light scan test

プロジェクタ側でpatternを順番に表示できる状態にしてから実行します。

```bash
chmod +x scripts/run_structured_light_scan_test.sh
```

```bash
LEFT_CAMERA=0 \
RIGHT_CAMERA=2 \
PATTERN_COUNT=46 \
PROJECTOR_WIDTH=1920 \
PROJECTOR_HEIGHT=1080 \
./scripts/run_structured_light_scan_test.sh \
  "$HOME/Downloads/tooth-scan-test"
```

このスクリプトは各patternについて、ユーザーのEnter入力後に左右stereo captureを行います。

自動でpatternを進めるコマンドがある場合:

```bash
PATTERN_ADVANCE_COMMAND='/path/to/advance-pattern.sh {index}' \
./scripts/run_structured_light_scan_test.sh \
  "$HOME/Downloads/tooth-scan-test"
```

`{index}`は0始まりのpattern番号へ置換されます。

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

## Scan datasetのdecode

scan撮影後、backendを起動して次を送信します。

```json
{"id":"scan-validate","cmd":"scan_validate","input_dir":"/path/to/tooth-scan-test","projector_width":1920,"projector_height":1080,"pattern_count":46}
```

decode:

```json
{"id":"scan-decode","cmd":"decode_patterns","input_dir":"/path/to/tooth-scan-test","output_dir":"/tmp/tooth-scan-decode","projector_width":1920,"projector_height":1080,"pattern_count":46,"threshold":1,"allow_partial":false}
```

## 注意事項

- 左右カメラの位置と角度は、stereo calibration後に変更しないでください。
- scanとcalibrationは同じ画像解像度で取得してください。
- auto exposureやauto focusによってpattern間の明るさや焦点が変化するとdecode品質が低下します。
- MJPEG stream確認中もcamera captureを共有するため、backend実装によっては一時的にframe取得が遅くなることがあります。
- Structured Light scanはpattern表示との同期が必要です。patternが切り替わる前にcaptureしないよう待機時間を確保してください。
