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

配布物:

- [tooth-reconstruction-fixture-v1.zip](https://drive.google.com/file/d/1Gm6vJCaQ6eL5y25IezgKyYi8kF1xMvdW/view?usp=sharing)
- [tooth-reconstruction-fixture-v1.zip.sha256](https://drive.google.com/file/d/1W_ru6hRdH0rYD__1yXI1zvDtKgEWigKI/view?usp=sharing)

### コマンドで取得する

```bash
FIXTURE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/tooth-project-tools"
FIXTURE_FILE="$FIXTURE_DIR/tooth-reconstruction-fixture-v1.zip"
FIXTURE_CHECKSUM_FILE="${FIXTURE_FILE}.sha256"

mkdir -p "$FIXTURE_DIR"

curl -L \
  'https://drive.usercontent.google.com/download?id=1Gm6vJCaQ6eL5y25IezgKyYi8kF1xMvdW&export=download&confirm=t' \
  -o "$FIXTURE_FILE"

curl -L \
  'https://drive.usercontent.google.com/download?id=1W_ru6hRdH0rYD__1yXI1zvDtKgEWigKI&export=download&confirm=t' \
  -o "$FIXTURE_CHECKSUM_FILE"
```

### ZIP全体のchecksumを確認する

```bash
(
  cd "$FIXTURE_DIR"
  sha256sum -c "$(basename "$FIXTURE_CHECKSUM_FILE")"
)
```

正常な場合:

```text
tooth-reconstruction-fixture-v1.zip: OK
```

### 取得結果を確認する

```bash
file "$FIXTURE_FILE"

ls -lh \
  "$FIXTURE_FILE" \
  "$FIXTURE_CHECKSUM_FILE"
```

## Smoke test

以降のコマンドはリポジトリルートで実行します。

Dev Container内では通常、次のディレクトリです。

```bash
cd /workspace
```

現在位置を確認します。

```bash
pwd
```

期待値:

```text
/workspace
```

実行権限を付与します。

```bash
chmod +x scripts/check_reconstruction_fixture.sh
```

fixture testを実行します。

```bash
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

出力先を引数で指定します。

```bash
./scripts/package_reconstruction_fixture.sh \
  "$HOME/tooth-reconstruction-fixture-v1.zip"
```

生成物:

```text
$HOME/tooth-reconstruction-fixture-v1.zip
$HOME/tooth-reconstruction-fixture-v1.zip.sha256
```

生成後、checksumを確認します。

```bash
(
  cd "$HOME"
  sha256sum -c tooth-reconstruction-fixture-v1.zip.sha256
)
```

生成されるZIPと`.sha256`は別媒体へ配置し、Gitには追加しません。
