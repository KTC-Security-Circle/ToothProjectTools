# Tauri sidecar MJPEG service

## 目的

ToothProjectTools のC++プロセスをTauriに同梱する常駐sidecarとして起動し、制御と映像配信を分離する。

```text
Tauri Web UI
  -> Tauri Rust backend
  -> stdin JSON Lines
  -> C++ tooth-backend sidecar
  -> OpenCV CameraManager
  -> localhost MJPEG server
  -> Tauri WebView <img>
```

## 所有権

| 対象 | 所有者 |
| --- | --- |
| camera device | C++ / OpenCV |
| camera frame取得 | C++ |
| capture / scan / calibration | C++ |
| MJPEG配信 | C++ |
| sidecar processの起動・停止 | Tauri Rust |
| command生成 | Tauri Web UI / Rust |
| response/event中継 | Tauri Rust |
| preview表示 | Tauri WebView |

Tauri/Rustはcamera deviceを直接openしない。同一deviceを複数runtimeからopenして所有権が競合する状態を作らない。

既存のOpenCV `WindowManager` はdebug GUI、projector、既存preview用として維持する。MJPEGは追加の出力先であり、既存window経路を置き換えない。

## 起動

```sh
./tooth-backend serve \
  --control stdio \
  --mjpeg-host 127.0.0.1 \
  --mjpeg-port 39010
```

MVPで利用できるcontrol transportは `stdio` のみ。MJPEG hostは `127.0.0.1` のみ受け付ける。`0.0.0.0` や外部interfaceへのbindは拒否する。

MJPEG serverのlisten開始後、sidecarはstdoutへready eventを1行出力する。

```json
{"event":"ready","version":"0.1.0"}
```

Tauri Rustはready受信前にcamera commandを送信しない。起動失敗またはready受信timeout時はprocessを停止し、UIへ起動エラーを通知する。

## stdio protocol

stdinとstdoutはUTF-8 JSON Linesとして扱う。1 command、1 response、1 eventをそれぞれ改行区切りの1行で送る。

- stdin: Tauri RustからC++へのcommand
- stdout: C++からTauri Rustへのresponse/event
- stderr: logger、OpenCV診断、process障害情報
- stdoutへJPEG bytesやMJPEG multipart bodyを出力しない
- responseはrequestと同じ文字列 `id` を持つ
- eventは非同期通知として `event` を持つ

成功response:

```json
{"id":"1","ok":true}
```

失敗response:

```json
{"id":"2","ok":false,"error":{"code":"camera_open_failed","message":"failed to open camera 0"}}
```

JSONをparseできず `id` も復元できない場合、error responseには `id` を付けない。

```json
{"ok":false,"error":{"code":"invalid_json","message":"failed to parse JSON"}}
```

## Commands

### ping

```json
{"id":"1","cmd":"ping"}
{"id":"1","ok":true,"result":"pong"}
```

### open_camera

`camera_id` はOpenCVへ渡すdevice index。`role` はstream endpointとC++内のcamera bindingに使う。

```json
{"id":"2","cmd":"open_camera","camera_id":0,"role":"left"}
{"id":"2","ok":true}
{"event":"camera_opened","camera_id":0,"role":"left"}
```

```json
{"id":"2","cmd":"open_camera","camera_id":2,"role":"right"}
{"id":"2","ok":true}
{"event":"camera_opened","camera_id":2,"role":"right"}
```

通常のcamera openに失敗した場合、dummy frameへ自動fallbackしない。

### close_camera

```json
{"id":"3","cmd":"close_camera","role":"left"}
{"id":"3","ok":true}
```

```json
{"id":"3","cmd":"close_camera","role":"right"}
{"id":"3","ok":true}
```

stream中のroleをcloseした場合はpublisherを停止してからcameraを解放する。

### start_stream

```json
{"id":"4","cmd":"start_stream","role":"left"}
{"id":"4","ok":true,"url":"http://127.0.0.1:39010/left.mjpg"}
{"event":"stream_started","role":"left","url":"http://127.0.0.1:39010/left.mjpg"}
```

```json
{"id":"4","cmd":"start_stream","role":"right"}
{"id":"4","ok":true,"url":"http://127.0.0.1:39010/right.mjpg"}
{"event":"stream_started","role":"right","url":"http://127.0.0.1:39010/right.mjpg"}
```

cameraがopen済みであることが前提。MVPのpublisherはデフォルト約60fps、JPEG quality 80で最新frameを配信する。

### stop_stream

```json
{"id":"5","cmd":"stop_stream","role":"left"}
{"id":"5","ok":true}
```

```json
{"id":"5","cmd":"stop_stream","role":"right"}
{"id":"5","ok":true}
```

### capture_frame

```json
{"id":"6","cmd":"capture_frame","role":"left","output":"./data/mono_left/001.png"}
{"id":"6","ok":true,"path":"./data/mono_left/001.png"}
{"event":"frame_saved","role":"left","path":"./data/mono_left/001.png"}
```

```json
{"id":"6","cmd":"capture_frame","role":"right","output":"./data/mono_right/002.png"}
{"id":"6","ok":true,"path":"./data/mono_right/002.png"}
{"event":"frame_saved","role":"right","path":"./data/mono_right/002.png"}
```

保存先の親directoryが存在しない場合はC++側で作成する。frame未取得時は `empty_frame`、書き込み失敗時は `file_write_failed` を返す。

### capture_stereo

左右roleに紐づくcameraから近いタイミングでframeを取得し、それぞれ保存する。

```json
{"id":"7","cmd":"capture_stereo","left_role":"left","right_role":"right","left_output":"./data/stereo/left_001.png","right_output":"./data/stereo/right_001.png"}
{"id":"7","ok":true,"left_path":"./data/stereo/left_001.png","right_path":"./data/stereo/right_001.png"}
{"event":"stereo_frame_saved","left_role":"left","right_role":"right","left_path":"./data/stereo/left_001.png","right_path":"./data/stereo/right_001.png"}
```

`left_role` / `right_role` はC++側でopen済みcamera idへ解決する。未openの場合は `camera_not_open` を返す。

### calib_capture_frame

calibration用の単眼画像を撮影・保存する。calibration計算は行わず、内部的には通常のcaptureと同じCaptureServiceを使う。

```json
{"id":"8","cmd":"calib_capture_frame","role":"left","output":"./data/calib/mono_left/001.png"}
{"id":"8","ok":true,"path":"./data/calib/mono_left/001.png","purpose":"calibration"}
{"event":"calibration_frame_saved","role":"left","path":"./data/calib/mono_left/001.png"}
```

### calib_capture_stereo

calibration用の左右画像ペアを撮影・保存する。calibration計算は行わず、内部的には通常のstereo captureと同じCaptureServiceを使う。

```json
{"id":"9","cmd":"calib_capture_stereo","left_role":"left","right_role":"right","left_output":"./data/calib/stereo/left_001.png","right_output":"./data/calib/stereo/right_001.png"}
{"id":"9","ok":true,"left_path":"./data/calib/stereo/left_001.png","right_path":"./data/calib/stereo/right_001.png","purpose":"calibration"}
{"event":"calibration_stereo_frame_saved","left_role":"left","right_role":"right","left_path":"./data/calib/stereo/left_001.png","right_path":"./data/calib/stereo/right_001.png"}
```

### shutdown

```json
{"id":"10","cmd":"shutdown"}
{"id":"10","ok":true}
```

responseをflushした後、publisher、camera、MJPEG serverを停止し、processは終了code 0で終了する。stdinがEOFになった場合も同じcleanupを行う。

## MJPEG endpoint

role `left` と `right` をopen/startした場合のURL:

```text
http://127.0.0.1:39010/left.mjpg
http://127.0.0.1:39010/right.mjpg
```

response content type:

```http
Content-Type: multipart/x-mixed-replace; boundary=frame
```

各partは次のheaderとJPEG bytesを持つ。

```http
--frame
Content-Type: image/jpeg
Content-Length: <bytes>
```

未登録または停止中のroleはHTTP 404を返す。endpoint名に利用できるrole文字は英数字、`_`、`-` のみ。

Web UIはsidecarが返したURLをそのまま利用する。

```html
<img src="http://127.0.0.1:39010/left.mjpg" alt="Left camera preview">
```

## Process lifecycle

1. Tauri Rustが利用可能なlocalhost portを決定する。
2. Rustが `tooth-backend serve` をchild processとして起動する。
3. Rustがstdin writer、stdout JSON Lines reader、stderr log readerを保持する。
4. Rustがready eventを待つ。
5. UI操作をid付きcommandへ変換してstdinへ送る。
6. Rustがresponseをrequest Promiseへ対応付け、eventをUIへemitする。
7. application終了時は `shutdown` を送り、正常終了を一定時間待つ。
8. timeoutまたは異常終了時のみRust側でchild processを強制停止する。

## Error codes

| code | 意味 |
| --- | --- |
| `invalid_json` | JSON parse失敗またはfield型不正 |
| `invalid_command` | 未対応commandまたは不正role |
| `missing_field` | commandの必須field不足 |
| `camera_open_failed` | OpenCV camera open失敗 |
| `camera_not_open` | roleにopen済みcameraがない |
| `stream_start_failed` | frame publisher開始失敗 |
| `empty_frame` | cameraから取得したframeが空 |
| `invalid_output_path` | 保存先pathが不正 |
| `directory_create_failed` | 保存先directory作成失敗 |
| `file_write_failed` | image保存失敗 |
| `capture_failed` | capture詳細errorがない失敗 |
| `internal_error` | command処理中の予期しない例外 |

## Dispatch integration

MVPでは `ControlInputAdapter` がJSON commandをsidecar専用serviceへ変換する。既存 `dispatch::execute` はGUIの `runtime::AppContext` とwindow targetへ強く依存しているため、sidecarからは直接利用しない。

後続作業ではheadless用contextとcamera command型を定義し、次の経路へ統合する。

```text
JSON Lines
  -> ControlInputAdapter
  -> DispatchCmd
  -> Dispatcher
  -> Handler
  -> Service
```

この統合でもcamera deviceとstreamの所有者はC++のままとする。
