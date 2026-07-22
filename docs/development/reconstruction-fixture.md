# Reconstruction Fixture

このドキュメントでは、`ToothProjectTools`で次の処理を確認する方法を説明します。

1. Mono calibration
2. Stereo calibration
3. GrayCode画像のdecode
4. 再構成入力の検証
5. PLY点群の生成
6. fixtureを使用したsmoke test

カメラのopen、stream、scan画像の撮影方法については、このドキュメントでは扱いません。

## Fixtureのダウンロード

再構成テスト用fixtureはGitリポジトリには含まれていません。

次のGoogle Driveからダウンロードしてください。

- [tooth-reconstruction-fixture-v1.zip](https://drive.google.com/file/d/19mctoKUiOA4YyCviAxavXwbGoCNjAB0m/view?usp=sharing)

コマンドで取得する場合:

```bash
FIXTURE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/tooth-project-tools"
FIXTURE_FILE="${FIXTURE_DIR}/tooth-reconstruction-fixture-v1.zip"

mkdir -p "$FIXTURE_DIR"

curl -L \
  'https://drive.usercontent.google.com/download?id=19mctoKUiOA4YyCviAxavXwbGoCNjAB0m&export=download&confirm=t' \
  -o "$FIXTURE_FILE"
```

ダウンロード結果を確認します。

```bash
file "$FIXTURE_FILE"
ls -lh "$FIXTURE_FILE"
```

期待する形式:

```text
Zip archive data
```

Google Driveの確認ページがHTMLとして保存された場合は、ブラウザから共有リンクを開いてダウンロードしてください。

## Fixtureの構成

ZIPを展開せずに確認します。

```bash
unzip -l "$FIXTURE_FILE" | sed -n '1,80p'
```

fixtureは次の構成です。

```text
tooth-reconstruction-fixture-v1/
├── README.md
├── manifest.json
├── SHA256SUMS
├── calibration/
│   ├── mono_L/
│   ├── mono_R/
│   ├── stereo_L/
│   └── stereo_R/
└── scan/
    ├── left/
    └── right/
```

主なデータ条件:

| 項目 | 値 |
| --- | --- |
| Camera image | 1024 × 768 |
| Projector | 1920 × 1080 |
| Pattern count | 46 |
| Decode threshold | 1 |
| Partial dataset | false |

## Fixture内部のchecksum確認

一時ディレクトリへ展開します。

```bash
FIXTURE_WORK_DIR="$(mktemp -d)"

unzip -q \
  "$FIXTURE_FILE" \
  -d "$FIXTURE_WORK_DIR"
```

内部checksumを確認します。

```bash
cd "$FIXTURE_WORK_DIR/tooth-reconstruction-fixture-v1"

sha256sum -c SHA256SUMS
```

すべてのファイルで`OK`が表示されることを確認してください。

確認後:

```bash
cd -
rm -rf "$FIXTURE_WORK_DIR"
```

## OpenCV 4.10 static build

OpenCV 4.10のstatic buildを作成します。

```bash
chmod +x scripts/build_opencv_4_10_static.sh

./scripts/build_opencv_4_10_static.sh
```

デフォルトのインストール先:

```text
~/.local/opencv/4.10.0-static
```

主な生成物を確認します。

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

リポジトリルートで実行します。

```bash
cmake --preset release-opencv-4.10-static
```

```bash
cmake \
  --build \
  --preset release-opencv-4.10-static \
  --parallel
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

## Smoke test

fixtureを使用した一連の確認は、`check_reconstruction_fixture.sh`で実行します。

```bash
chmod +x scripts/check_reconstruction_fixture.sh
```

```bash
./scripts/check_reconstruction_fixture.sh \
  "$FIXTURE_FILE"
```

backendがデフォルトパス以外にある場合:

```bash
TOOTH_BACKEND=/path/to/tooth-backend \
  ./scripts/check_reconstruction_fixture.sh \
  "$FIXTURE_FILE"
```

正常終了時:

```text
PASS: reconstruction fixture
```

smoke testでは、次の処理を順番に確認します。

1. fixture ZIPの展開
2. fixture内部のSHA-256検証
3. `tooth-backend serve`の起動
4. `ping`
5. Left mono calibration
6. Right mono calibration
7. Stereo calibration
8. GrayCode decode
9. Reconstruction validation
10. Point-cloud reconstruction
11. PLY headerとvertex数の検証
12. NaNおよびInfの検査
13. backendのshutdown

## 手動テスト

smoke testで問題が発生した場合は、各コマンドを手動で実行して切り分けます。

### Fixtureの展開

```bash
FIXTURE_WORK_DIR="$(mktemp -d)"

unzip -q \
  "$FIXTURE_FILE" \
  -d "$FIXTURE_WORK_DIR"

FIXTURE_ROOT="$FIXTURE_WORK_DIR/tooth-reconstruction-fixture-v1"
OUTPUT_ROOT="$FIXTURE_WORK_DIR/output"

mkdir -p \
  "$OUTPUT_ROOT/calibration" \
  "$OUTPUT_ROOT/decode" \
  "$OUTPUT_ROOT/reconstruction"
```

### Backendの起動

リポジトリルートで実行します。

```bash
build/release-opencv-4.10-static/src/serve/tooth-backend \
  serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

ready eventが表示されることを確認します。

```json
{"event":"ready","version":"0.1.0"}
```

### Ping

```json
{"id":"fixture-ping","cmd":"ping"}
```

正常応答:

```json
{"id":"fixture-ping","ok":true}
```

### Left mono calibration

以下のパスは、実際の`FIXTURE_WORK_DIR`へ置き換えてください。

```json
{"id":"fixture-mono-left","cmd":"mono_calibrate","image_folder":"/tmp/FIXTURE/tooth-reconstruction-fixture-v1/calibration/mono_L","output_file":"/tmp/FIXTURE/output/calibration/mono_left.yml"}
```

`ok:true`になり、次のファイルが生成されることを確認します。

```text
output/calibration/mono_left.yml
```

### Right mono calibration

```json
{"id":"fixture-mono-right","cmd":"mono_calibrate","image_folder":"/tmp/FIXTURE/tooth-reconstruction-fixture-v1/calibration/mono_R","output_file":"/tmp/FIXTURE/output/calibration/mono_right.yml"}
```

生成物:

```text
output/calibration/mono_right.yml
```

### Stereo calibration

```json
{"id":"fixture-stereo","cmd":"stereo_calibrate","left_dir":"/tmp/FIXTURE/tooth-reconstruction-fixture-v1/calibration/stereo_L","right_dir":"/tmp/FIXTURE/tooth-reconstruction-fixture-v1/calibration/stereo_R","left_calibration_file":"/tmp/FIXTURE/output/calibration/mono_left.yml","right_calibration_file":"/tmp/FIXTURE/output/calibration/mono_right.yml","output_file":"/tmp/FIXTURE/output/calibration/stereo.yml"}
```

生成物:

```text
output/calibration/stereo.yml
```

画像サイズを確認します。

```bash
grep -E '^image_width:|^image_height:' \
  "$OUTPUT_ROOT/calibration/stereo.yml"
```

期待値:

```text
image_width: 1024
image_height: 768
```

### GrayCode decode

```json
{"id":"fixture-decode","cmd":"decode_patterns","input_dir":"/tmp/FIXTURE/tooth-reconstruction-fixture-v1/scan","output_dir":"/tmp/FIXTURE/output/decode","projector_width":1920,"projector_height":1080,"pattern_count":46,"threshold":1,"allow_partial":false}
```

正常時は、少なくとも次の値が0より大きくなります。

```text
left_valid_count
right_valid_count
```

生成物:

```text
output/decode/
├── metadata.json
├── left/
│   ├── projector_x.yml
│   ├── projector_y.yml
│   └── valid_mask.png
└── right/
    ├── projector_x.yml
    ├── projector_y.yml
    └── valid_mask.png
```

### Reconstruction validation

```json
{"id":"fixture-reconstruct-validate","cmd":"reconstruct_validate","decode_dir":"/tmp/FIXTURE/output/decode","calibration_file":"/tmp/FIXTURE/output/calibration/stereo.yml"}
```

正常時に確認する項目:

```text
ok: true
valid: true
issue_count: 0
image_width: 1024
image_height: 768
projector_width: 1920
projector_height: 1080
valid_correspondence_count > 0
reconstructable_point_count > 0
```

このfixtureでは、次の結果が確認されています。

```text
left_valid_count: 124670
right_valid_count: 88125
valid_correspondence_count: 8111
reconstructable_point_count: 8111
```

OpenCVのbuild条件や処理実装の変更によって数値が変化する可能性があるため、smoke testでは完全一致を必須にしません。

### Point-cloud reconstruction

```json
{"id":"fixture-reconstruct","cmd":"reconstruct_point_cloud","decode_dir":"/tmp/FIXTURE/output/decode","calibration_file":"/tmp/FIXTURE/output/calibration/stereo.yml","output_file":"/tmp/FIXTURE/output/reconstruction/cloud.ply"}
```

正常時:

```text
ok: true
point_count > 0
valid_correspondence_count > 0
warning_count: 0
```

このfixtureでは、次の点数が確認されています。

```text
point_count: 8111
```

### PLYの確認

```bash
test -s "$OUTPUT_ROOT/reconstruction/cloud.ply" &&
  echo "OK: cloud.ply"
```

header:

```bash
sed -n '1,15p' \
  "$OUTPUT_ROOT/reconstruction/cloud.ply"
```

期待する形式:

```text
ply
format ascii 1.0
element vertex 8111
property float x
property float y
property float z
end_header
```

NaNおよびInfを確認します。

```bash
grep -Ein '\bnan\b|\binf\b' \
  "$OUTPUT_ROOT/reconstruction/cloud.ply" &&
  echo "NG: invalid coordinate found" ||
  echo "OK: no NaN/Inf"
```

PLY headerに記録されたvertex数:

```bash
grep '^element vertex ' \
  "$OUTPUT_ROOT/reconstruction/cloud.ply"
```

### Shutdown

```json
{"id":"fixture-shutdown","cmd":"shutdown"}
```

## Fixtureの作成

この処理は、fixture元データを保持している管理者のみ実行します。

出力先を明示して実行してください。

```bash
chmod +x scripts/package_reconstruction_fixture.sh
```

```bash
./scripts/package_reconstruction_fixture.sh \
  "$HOME/Downloads/tooth-reconstruction-fixture-v1.zip"
```

生成物:

```text
~/Downloads/tooth-reconstruction-fixture-v1.zip
~/Downloads/tooth-reconstruction-fixture-v1.zip.sha256
```

生成後はZIPをGoogle Driveへアップロードし、このドキュメントのリンクを更新してください。

fixture ZIP、元画像、生成されたcalibration、decode結果、PLYはGitリポジトリへ追加しません。

## トラブルシューティング

### `image_size_mismatch`

decode画像とstereo calibrationの画像サイズが一致していません。

確認:

```bash
cat "$OUTPUT_ROOT/decode/metadata.json"

grep -E '^image_width:|^image_height:' \
  "$OUTPUT_ROOT/calibration/stereo.yml"
```

両方が次である必要があります。

```text
1024 × 768
```

画像を単純にresizeして合わせるのではなく、同じカメラ設定と解像度で取得したcalibration画像を使用してください。

### `pattern_count_not_found`

scan画像の名前を確認してください。

```text
pattern_000.png
pattern_001.png
...
pattern_045.png
```

確認:

```bash
find "$FIXTURE_ROOT/scan" \
  -maxdepth 2 \
  -type f \
  -name 'pattern_*.png' |
sort
```

### `projector_width/projector_height are required`

fixtureのscan datasetには、projector情報を持つscan用`metadata.json`が含まれていない場合があります。

`decode_patterns`へ次を指定してください。

```json
{
  "projector_width": 1920,
  "projector_height": 1080,
  "pattern_count": 46
}
```

### `output_file_exists`

`reconstruct_point_cloud`は、デフォルトでは既存PLYを上書きしません。

既存ファイルを削除します。

```bash
rm -f "$OUTPUT_ROOT/reconstruction/cloud.ply"
```

または`overwrite:true`を指定します。

```json
{"id":"fixture-reconstruct","cmd":"reconstruct_point_cloud","decode_dir":"/tmp/FIXTURE/output/decode","calibration_file":"/tmp/FIXTURE/output/calibration/stereo.yml","output_file":"/tmp/FIXTURE/output/reconstruction/cloud.ply","overwrite":true}
```

### `insufficient_valid_correspondence`

左右のdecode結果から、有効なprojector対応点を十分に取得できていません。

次を確認してください。

- 左右scan画像のファイル名と順序
- pattern数が左右とも46枚であること
- decode threshold
- 左右のvalid mask
- scanとcalibrationの画像解像度
- scan撮影後に左右カメラの位置が変更されていないこと

## 対象外

次の操作は別ドキュメントで管理します。

- カメラデバイスの列挙
- カメラのopenとclose
- カメラ解像度の設定
- MJPEG streamの開始と停止
- GrayCode patternの表示
- scan sessionの開始
- scan画像の撮影
- scan datasetの保存