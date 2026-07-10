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


### open_window

```json
{"id":"40","cmd":"open_window","window_role":"projector","title":"Projector","width":1920,"height":1080,"monitor_index":1,"fullscreen":true}
{"id":"40","ok":true,"window_role":"projector","window_id":"1","width":"1920","height":"1080"}
{"event":"window_opened","window_role":"projector","window_id":"1","width":"1920","height":"1080"}
```

- `window_role` はruntime内のwindow binding名。
- `title` 省略時は `window_role` を使う。
- `width` / `height` は必須。
- `monitor_index` 省略時は既定monitorを使う。
- `fullscreen` 省略時は `false`。
- 現段階では既存 `WindowManager` backendを利用する。

### close_window

```json
{"id":"41","cmd":"close_window","window_role":"projector"}
{"id":"41","ok":true,"window_role":"projector","window_id":"1"}
{"event":"window_closed","window_role":"projector","window_id":"1"}
```

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

### mono_calibrate

保存済み単眼calibration画像からcamera intrinsicsを計算する。

```json
{"id":"20","cmd":"mono_calibrate","role":"left","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml"}
{"id":"20","ok":true,"role":"left","image_folder":"./data/calib/mono_left","output_file":"./data/calib/mono_left.yml","rms":"0.420000"}
{"event":"mono_calibration_finished","role":"left","output_file":"./data/calib/mono_left.yml","rms":"0.420000"}
```

`output_file` 省略時は `./data/calib/<role>_mono.yml` に保存する。calibration画像が不足している場合や結果fileを書けない場合は失敗responseを返す。

### stereo_calibrate

保存済み左右calibration画像pairからstereo calibrationを計算する。

```json
{"id":"21","cmd":"stereo_calibrate","left_role":"left","right_role":"right","left_dir":"./data/calib/stereo/left","right_dir":"./data/calib/stereo/right","output_file":"./data/calib/stereo.yml"}
{"id":"21","ok":true,"left_role":"left","right_role":"right","left_dir":"./data/calib/stereo/left","right_dir":"./data/calib/stereo/right","output_file":"./data/calib/stereo.yml","rms":"0.620000"}
{"event":"stereo_calibration_finished","left_role":"left","right_role":"right","output_file":"./data/calib/stereo.yml","rms":"0.620000"}
```

左右roleが同じcameraへ解決される場合、または `left_dir` と `right_dir` が同じpathを指す場合は `invalid_command` を返す。

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
| `window_close_failed` | WindowManager backendでwindow closeに失敗 |
| `window_not_open` | roleにopen済みwindowがない |
| `window_open_failed` | WindowManager backendでwindow作成に失敗 |
| `window_already_open` | 指定window_roleが既にopen済み |
| `invalid_monitor_index` | monitor_indexが不正 |
| `invalid_window_size` | window width/heightが不正 |
| `invalid_window_role` | window_roleが空、または英数字、`_`、`-` 以外を含む |
| `capture_failed` | capture詳細errorがない失敗 |
| `calibration_failed` | mono calibration計算失敗 |
| `stereo_calibration_failed` | stereo calibration計算失敗 |
| `calibration_image_not_found` | calibration画像directoryまたは画像が見つからない |
| `calibration_image_count_mismatch` | stereo calibration左右画像数不一致 |
| `calibration_output_write_failed` | calibration結果file書き込み失敗 |
| `internal_error` | command処理中の予期しない例外 |

## Dispatch integration

`open_camera` / `close_camera` は `CmdOpenCamera` / `CmdCloseCamera` として `HeadlessCommandMapper` で変換され、`HeadlessDispatcher` へ渡され、`CameraHandler` から `CameraService` を呼び出して実行する。

`SidecarService` はcamera open/closeの本体を持たず、MJPEG publisher、stream URL、process lifecycle、close前のstream停止などsidecar固有の統合処理だけを担当する。

GUI向けの既存 `dispatch::execute` は `runtime::AppContext` とwindow targetへ強く依存するため、sidecarではGUI非依存の `HeadlessDispatcher` を利用する。

現在のcamera resource commandは次の経路で処理する。

```text
JSON Lines
  -> ControlInputAdapter
  -> HeadlessCommandMapper
  -> HeadlessDispatcher
  -> CameraHandler
  -> CameraService
  -> CameraManager
```

この統合でもcamera deviceとstreamの所有者はC++のままとする。


Window resource commandは次の経路で処理する。

```text
JSON Lines ControlMessage
  -> ControlInputAdapter
  -> HeadlessCommandMapper
  -> cmd::CmdOpenWindow / cmd::CmdCloseWindow
  -> HeadlessDispatcher
  -> WindowResourceHandler
  -> WindowService
  -> WindowManager
  -> common::CommandResult
  -> ControlResponse
```

| JSONL `cmd` | C++ command | Handler | Service | 備考 |
| --- | --- | --- | --- | --- |
| `open_window` | `cmd::CmdOpenWindow` | `handler::window_resource` | `service::window::WindowService` | WindowManagerを通してwindowを作成しroleへbindする |
| `close_window` | `cmd::CmdCloseWindow` | `handler::window_resource` | `service::window::WindowService` | roleに紐づくwindowをcloseする |

mapping例:

```text
ControlMessage
  cmd = "open_window"
  window_role = "projector"
  width = 1920
  height = 1080

HeadlessCommandMapper
  -> cmd::CmdOpenWindow

HeadlessDispatcher
  -> WindowResourceHandler
  -> WindowService
  -> WindowManager
```
