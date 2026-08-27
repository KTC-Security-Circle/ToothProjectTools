# Structured Light Scan デモ

このページでは、左右のカメラとProjectorを使用して、Structured Light Scanを開始するまでのcontrol command実行手順を説明する。

個別コマンドの仕様確認ではなく、上から順に実行するデモ用手順である。

## 前提条件

- `tooth-backend serve`が起動している
- control transportとしてstdioを使用している
- 左右のカメラが接続されている
- ProjectorまたはProjector表示用モニターが接続されている
- 使用するcamera IDとmonitor indexを確認している
- 出力先へ書き込み可能である

起動例:

```bash
./build/release-opencv-4.10-static/src/serve/tooth-backend \
  serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

起動後、次のeventを確認する。

```json
{"event":"ready","version":"0.1.0"}
```

## 実行順序

次の順序を変更しないこと。

```text
open_camera
  -> open_window
  -> open_projector
  -> set_surface
  -> generate_pattern
  -> scan_start
```

各コマンドについて、成功responseと必要な完了eventを確認してから次のコマンドへ進むこと。

## 1. 接続確認

Request:

```json
{"id":"1","cmd":"ping"}
```

期待するresponse:

```json
{"id":"1","ok":true}
```

## 2. 左カメラを開く

Request:

```json
{
  "id": "10",
  "cmd": "open_camera",
  "camera_id": 0,
  "role": "left"
}
```

`ok=true`のresponseと、左カメラのopen完了eventを確認する。

## 3. 右カメラを開く

Request:

```json
{
  "id": "11",
  "cmd": "open_camera",
  "camera_id": 2,
  "role": "right"
}
```

`ok=true`のresponseと、右カメラのopen完了eventを確認する。

## 4. Projector用Windowを開く

既存の`open_window`コマンド仕様に従って、Projector表示用Windowを作成する。

Request:

```json
{
  "id": "20",
  "cmd": "open_window",
  "window_role": "projector",
  "width": 1920,
  "height": 1080
}
```

期待するeventの例:

```json
{
  "event": "window_opened",
  "window_role": "projector",
  "window_id": "1",
  "width": "1920",
  "height": "1080"
}
```

このeventはWindowの作成完了を示す。

Projector自体のopen完了を示すものではないため、続けて`open_projector`を実行すること。

## 5. Projectorを開く

既存の`open_projector`コマンド仕様に従い、前段で作成したWindowをProjectorとして開く。`width` / `height` はGray Code論理解像度であり、display regionのサイズではない。

Request:

```json
{
  "id": "30",
  "cmd": "open_projector",
  "projector_role": "projector",
  "window_role": "projector",
  "width": 960,
  "height": 540
}
```

`ok=true`のresponseと、Projectorのopen完了eventを確認する。

`projector_not_open`が返る場合は、この手順が完了していない。

## 6. Projector surfaceを設定する

既存のsurface設定コマンド仕様に従う。

Request:

```json
{
  "id": "40",
  "cmd": "configure_projector_surface",
  "projector_role": "projector",
  "monitor_index": 0,
  "width": 1920,
  "height": 1080,
  "placement": "center"
}
```

期待するresponse:

```json
{
  "id": "40",
  "ok": true
}
```

デモでは、Gray Code論理解像度と表示解像度を分ける。960 x 540で生成したpatternを、表示時だけ1920 x 1080のdisplay regionへnearest-neighborで拡大する。

```text
Code resolution:    960 x 540
Display region:    1920 x 1080
Expected patterns: 42
```

## 7. Scan patternを生成する

既存のpattern生成コマンド仕様に従い、Scanで使用するpatternを生成する。生成解像度は`open_projector`で指定したGray Code論理解像度である。

Request:

```json
{
  "id": "50",
  "cmd": "generate_patterns",
  "projector_role": "projector"
}
```

`ok=true`のresponseと、pattern生成完了を示すresponseまたはeventを確認する。この例では `width=960`、`height=540`、`code_width=960`、`code_height=540`、`display_width=1920`、`display_height=1080`、`pattern_count=42` を期待する。

patternの生成が完了する前に`scan_start`を送信しないこと。

## 8. Scanを開始する

Request:

```json
{
  "id": "70",
  "cmd": "scan_start",
  "scan_id": "session_001",
  "left_role": "left",
  "right_role": "right",
  "projector_role": "projector",
  "output_dir": "./data/scan/session_001",
  "settle_ms": 120
}
```

期待するresponse:

```json
{
  "id": "70",
  "ok": true
}
```

Scan開始後は、完了または失敗を示すeventを待つ。

Scan中に、同じ`scan_id`を使用して再度`scan_start`を実行しないこと。

## 9. 出力を確認する

```bash
find ./data/scan/session_001 \
  -maxdepth 3 \
  -type f \
  | sort
```

左右のpattern撮影画像が保存されていることを確認する。

## 10. 終了処理

Scanが完了した後、既存コマンド仕様に従って次の順序でリソースを閉じる。

```text
close_projector
  -> close_window
  -> close_camera right
  -> close_camera left
  -> shutdown
```

最後にbackendを終了する。

```json
{"id":"99","cmd":"shutdown"}
```

## 主なエラー

### projector_not_open

エラー例:

```json
{
  "code": "projector_not_open",
  "message": "projector role is not open: projector"
}
```

主な原因:

- `open_projector`を実行していない
- `open_projector`が失敗している
- `projector_role`の指定が一致していない
- `open_window`だけを実行して`set_surface`または`scan_start`へ進んでいる

次の実行順序を確認する。

```text
open_window
  -> open_projector
  -> set_surface
```

### camera_not_open

左右どちらかのcamera roleがopen状態になっていない。

`scan_start`の前に、`left`と`right`の両方についてopen成功responseを確認する。

### pattern未生成

`generate_pattern`の完了前に`scan_start`を実行している。

pattern生成成功responseまたは完了eventを確認してから、`scan_start`を送信する。

## コマンド仕様との整合性

このページに記載する次の項目は、各コマンドの正式な仕様と一致させること。

- `open_window`の引数
- `open_projector`の引数
- surface設定コマンドの正式名称と引数
- pattern生成コマンドの正式名称と引数
- 各コマンドの成功response
- 各処理の完了event名
- 終了処理に使用するcommand名

個別コマンドの仕様を変更した場合は、このデモ手順も更新すること。