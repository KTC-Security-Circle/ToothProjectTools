# Reconstruction Fixture

このドキュメントでは、配布fixtureを使って次の処理を確認します。

1. Left/Right mono calibration
2. Stereo calibration
3. GrayCode decode
4. Reconstruction validation
5. PLY point-cloud生成

カメラのopen、stream、scan撮影は別ドキュメントで扱います。

- [OpenCV 4.10 static build](./opencv-4.10-static-build.md)
- [Camera・stream・scan smoke test](./camera-stream-scan-smoke-test.md)

## Fixtureの取得

fixtureはGitリポジトリには含まれていません。

- [tooth-reconstruction-fixture-v1.zip](https://drive.google.com/file/d/19mctoKUiOA4YyCviAxavXwbGoCNjAB0m/view?usp=sharing)

コマンドで取得する場合:

```bash
FIXTURE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/tooth-project-tools"
FIXTURE_FILE="$FIXTURE_DIR/tooth-reconstruction-fixture-v1.zip"

mkdir -p "$FIXTURE_DIR"

curl -L \
  'https://drive.usercontent.google.com/download?id=19mctoKUiOA4YyCviAxavXwbGoCNjAB0m&export=download&confirm=t' \
  -o "$FIXTURE_FILE"
```

取得結果:

```bash
file "$FIXTURE_FILE"
ls -lh "$FIXTURE_FILE"
```

## Smoke test

```bash
chmod +x scripts/check_reconstruction_fixture.sh

./scripts/check_reconstruction_fixture.sh \
  "$FIXTURE_FILE"
```

backendが別パスの場合:

```bash
TOOTH_BACKEND=/path/to/tooth-backend \
  ./scripts/check_reconstruction_fixture.sh \
  "$FIXTURE_FILE"
```

正常終了:

```text
PASS: reconstruction fixture
```

## Fixture仕様

```text
camera image: 1024x768
projector: 1920x1080
pattern count: 46
decode threshold: 1
allow partial: false
```

確認済みの代表値:

```text
left_valid_count: 124670
right_valid_count: 88125
valid_correspondence_count: 8111
point_count: 8111
```

実装やOpenCV build条件による差を許容するため、smoke testでは完全一致ではなく次を確認します。

- validationの`valid`が`true`
- `issue_count`が`0`
- 対応点数が0より大きい
- 点群数が0より大きい
- PLY headerのvertex数とresponseのpoint数が一致する
- PLYにNaNまたはInfがない

## 手動実行

backend:

```bash
build/release-opencv-4.10-static/src/serve/tooth-backend \
  serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

Left mono calibration:

```json
{"id":"fixture-mono-left","cmd":"mono_calibrate","image_folder":"/path/to/fixture/calibration/mono_L","output_file":"/tmp/tooth-output/calibration/mono_left.yml"}
```

Right mono calibration:

```json
{"id":"fixture-mono-right","cmd":"mono_calibrate","image_folder":"/path/to/fixture/calibration/mono_R","output_file":"/tmp/tooth-output/calibration/mono_right.yml"}
```

Stereo calibration:

```json
{"id":"fixture-stereo","cmd":"stereo_calibrate","left_dir":"/path/to/fixture/calibration/stereo_L","right_dir":"/path/to/fixture/calibration/stereo_R","left_calibration_file":"/tmp/tooth-output/calibration/mono_left.yml","right_calibration_file":"/tmp/tooth-output/calibration/mono_right.yml","output_file":"/tmp/tooth-output/calibration/stereo.yml"}
```

Decode:

```json
{"id":"fixture-decode","cmd":"decode_patterns","input_dir":"/path/to/fixture/scan","output_dir":"/tmp/tooth-output/decode","projector_width":1920,"projector_height":1080,"pattern_count":46,"threshold":1,"allow_partial":false}
```

Validation:

```json
{"id":"fixture-validate","cmd":"reconstruct_validate","decode_dir":"/tmp/tooth-output/decode","calibration_file":"/tmp/tooth-output/calibration/stereo.yml"}
```

Point cloud:

```json
{"id":"fixture-reconstruct","cmd":"reconstruct_point_cloud","decode_dir":"/tmp/tooth-output/decode","calibration_file":"/tmp/tooth-output/calibration/stereo.yml","output_file":"/tmp/tooth-output/reconstruction/cloud.ply"}
```

## Fixtureの再作成

元データを保持する管理者のみ実行します。

```bash
./scripts/package_reconstruction_fixture.sh \
  "$HOME/Downloads/tooth-reconstruction-fixture-v1.zip"
```

生成されるZIPと`.sha256`は別媒体へ配置し、Gitには追加しません。
