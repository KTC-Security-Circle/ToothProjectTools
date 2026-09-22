# 入出力仕様（JSON Lines）

## 起動

```sh
./tooth-backend serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

`--control` は `stdio` を指定する。
MJPEG hostは `127.0.0.1` を使う。
stdoutへJPEG bytesを出さない。

## ready event

MJPEG serverのlisten開始後、serve appはstdoutへready eventを1行出す。

```json
{"event":"ready","version":"0.1.0"}
```

ready eventを受け取る前にcamera commandを送らない。

## stdout / stderr

```text
stdout:
  responseとeventをJSON Linesで出力する。

stderr:
  logger、OpenCV診断、process障害情報を出力する。
```

## request

stdinへ1行1JSONでrequestを送る。

```json
{"id":"1","cmd":"ping"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。returnに同じ値を入れる。 |
| `cmd` | 必須 | command名。 |

## 成功return

```json
{"id":"1","ok":true}
```

command固有のfieldを追加する。

## 失敗return

```json
{"id":"2","ok":false,"error":{"code":"camera_open_failed","message":"failed to open camera 0"}}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | `false`。 |
| `error.code` | error code。 |
| `error.message` | error message。 |

## JSON parse failure

JSONをparseできない場合、`id` を省略できる。

```json
{"ok":false,"error":{"code":"invalid_json","message":"failed to parse JSON"}}
```

## event形式

```json
{"event":"stream_started","role":"left","url":"http://127.0.0.1:39010/left.mjpg"}
```

eventは非同期通知である。
eventはrequestへのreturnではない。

## MJPEG endpoint

role `left` と `right` のURL例である。

```text
http://127.0.0.1:39010/left.mjpg
http://127.0.0.1:39010/right.mjpg
```

HTTP responseはmultipart MJPEGである。

```http
Content-Type: multipart/x-mixed-replace; boundary=frame
```

未登録または停止中のroleはHTTP 404を返す。
endpoint名に使えるrole文字は英数字、`_`、`-` である。

## ping

### 役割

JSON Linesの疎通を確認する。

### args(JSONL)

```json
{"id":"1","cmd":"ping"}
```

### return

```json
{"id":"1","ok":true,"result":"pong"}
```

### event

なし。

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

serve app。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `id` または `cmd` がない。 |
| `invalid_json` | JSONをparseできない。 |

## shutdown

### 役割

serve appの終了を要求する。

### args(JSONL)

```json
{"id":"10","cmd":"shutdown"}
```

### return

```json
{"id":"10","ok":true}
```

### event

scan停止中のeventが残っている場合は出力される。

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

serve app。

### error code

| code | 条件 |
| --- | --- |
| `internal_error` | 終了処理中に未処理例外が発生した。 |

## 実装済みcommand

- `ping`
- `shutdown`
- `open_camera`
- `close_camera`
- `start_stream`
- `stop_stream`
- `capture_frame`
- `capture_stereo`
- `calib_capture_frame`
- `calib_capture_stereo`
- `calib_detect_corners`
- `mono_calibrate`
- `stereo_calibrate`
- `camera_projector_calibrate`
- `open_window`
- `close_window`
- `list_monitors`
- `configure_projector_surface`
- `open_projector`
- `close_projector`
- `generate_patterns`
- `show_pattern`
- `next_pattern`
- `prev_pattern`
- `scan_start`
- `scan_status`
- `scan_stop`
- `scan_validate`
- `decode_patterns`
- `reconstruct_validate`
- `reconstruct_point_cloud`

## 未実装command

なし
