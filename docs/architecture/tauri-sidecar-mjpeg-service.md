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



### list_monitors

```json
{"id":"60","cmd":"list_monitors"}
{"id":"60","ok":true,"monitor_count":"2","monitors_json":"[{\"monitor_index\":0,\"x\":0,\"y\":0,\"width\":1920,\"height\":1080,\"primary\":true,\"name\":\"monitor-0\",\"fallback\":false}]"}
```

`monitor_index` は JSONL API、MonitorService、ProjectorService、WindowService public API では常に 0-based index。既存 `win::Window` backend が1-basedを要求する場合の変換は WindowManagerBackend 内だけで行う。monitor情報を取得できない環境では fallback として `monitor_index=0`, `x=0`, `y=0`, `width=1920`, `height=1080`, `primary=true`, `name=default`, `fallback=true` を返す。

### configure_projector_surface

中央配置:

```json
{"id":"61","cmd":"configure_projector_surface","projector_role":"projector","monitor_index":1,"width":1280,"height":720,"placement":"center"}
{"id":"61","ok":true,"projector_role":"projector","window_role":"projector","monitor_index":"1","monitor_x":"1920","monitor_y":"0","monitor_width":"1920","monitor_height":"1080","surface_width":"1920","surface_height":"1080","pattern_width":"1280","pattern_height":"720","pattern_x":"320","pattern_y":"180","clamped":"false"}
{"event":"projector_surface_configured","projector_role":"projector","window_role":"projector","monitor_index":"1","monitor_x":"1920","monitor_y":"0","monitor_width":"1920","monitor_height":"1080","surface_width":"1920","surface_height":"1080","pattern_width":"1280","pattern_height":"720","pattern_x":"320","pattern_y":"180","clamped":"false"}
```

custom配置:

```json
{"id":"62","cmd":"configure_projector_surface","projector_role":"projector","monitor_index":1,"width":1280,"height":720,"x":320,"y":180,"placement":"custom"}
```

- `monitor_size` は実monitorの表示可能最大領域。例: 1920x1080。
- `surface_size` は WindowService へ表示する canvas size。原則として monitor size と同じ。
- `pattern_size` は GrayCodePattern を生成する active area size。`width` / `height` は requested active pattern area の size。
- `pattern_origin` は surface 内で pattern を貼る左上座標。
- GrayCodePatternは active pattern size で生成される。
- 表示時に `surface_size` の canvas を作り、全画素を 0,0,0 の black canvas で初期化する。
- pattern ROI のみ GrayCodePattern で上書きする。ROI外は必ず黒。
- requested size が monitor size を超えた場合は pattern size を clamp する。
- custom `x` / `y` により pattern が monitor 外へ出る場合も、origin または pattern size を clamp する。
- clamp が発生した場合は response の `clamped=true`。
- surface変更後は GrayCodePattern の再生成が必要。`generate_patterns` は active pattern size で GrayCodePattern を生成する。
- projector surface設定時、WindowServiceはopen済みwindowをmonitor sizeへ移動、resize、fullscreen化する。現PRではWindowManager backendでmove/resize/fullscreenを試みる。backendがconfigureに失敗した場合、将来的にはclose/reopenへfallbackする。HighGUI操作はWindowServiceのmain thread queue上、またはGUI threadからの直接executeで実行される。
- `close_projector` は window を閉じない。window を閉じる場合は `close_window` を使う。

### open_projector

```json
{"id":"50","cmd":"open_projector","projector_role":"projector","window_role":"projector","width":1920,"height":1080}
{"id":"50","ok":true,"projector_role":"projector","window_role":"projector","width":"1920","height":"1080","surface_width":"1920","surface_height":"1080","pattern_width":"1920","pattern_height":"1080","pattern_x":"0","pattern_y":"0","clamped":"false"}
{"event":"projector_opened","projector_role":"projector","window_role":"projector","width":"1920","height":"1080"}
```

`projector_role` はruntime内のprojector binding名。`window_role` は既存の `open_window` で作成済みの表示先window role。`open_projector` の `width` / `height` は、初期状態の requested active pattern size として扱う。monitor情報が取得できる場合、表示surfaceはmonitor sizeになり、patternはsurface内のactive areaとして配置される。既存互換のため response の `width` / `height` は残すが、実際のsurface/pattern状態は `surface_width` / `surface_height` / `pattern_width` / `pattern_height` / `pattern_x` / `pattern_y` / `clamped` を参照する。現段階ではwindow backendのみを利用し、DRM/KMSやHDMI直接制御は後続PRで扱う。

### generate_patterns

```json
{"id":"51","cmd":"generate_patterns","projector_role":"projector"}
{"id":"51","ok":true,"projector_role":"projector","pattern_count":"44","width":"1920","height":"1080"}
{"event":"patterns_generated","projector_role":"projector","pattern_count":"44","width":"1920","height":"1080"}
```

ProjectorServiceは現在の ProjectorSurface の active pattern size で GrayCodePattern を生成する。surface未設定時は `open_projector` の `width` / `height` から default surface を作る。

### show_pattern

```json
{"id":"52","cmd":"show_pattern","projector_role":"projector","index":0}
{"id":"52","ok":true,"projector_role":"projector","pattern_index":"0"}
{"event":"pattern_shown","projector_role":"projector","pattern_index":"0"}
```

表示は `WindowService::showImage` 経由で行う。ProjectorServiceはWindowManagerやHighGUIを直接操作しない。active pattern は `surface_width` x `surface_height` の black canvas へ合成して表示する。

### next_pattern / prev_pattern

```json
{"id":"53","cmd":"next_pattern","projector_role":"projector"}
{"id":"53","ok":true,"projector_role":"projector","pattern_index":"1"}
{"event":"pattern_shown","projector_role":"projector","pattern_index":"1"}
```

```json
{"id":"54","cmd":"prev_pattern","projector_role":"projector"}
{"id":"54","ok":true,"projector_role":"projector","pattern_index":"0"}
{"event":"pattern_shown","projector_role":"projector","pattern_index":"0"}
```

### close_projector

```json
{"id":"55","cmd":"close_projector","projector_role":"projector"}
{"id":"55","ok":true,"projector_role":"projector"}
{"event":"projector_closed","projector_role":"projector"}
```

`close_projector` はprojector bindingだけを解除し、window自体は閉じない。windowを閉じる場合は `close_window` を使う。


### scan_start

```json
{"id":"70","cmd":"scan_start","scan_id":"session_001","projector_role":"projector","left_role":"left","right_role":"right","output_dir":"./data/scan/session_001","settle_ms":120}
{"id":"70","ok":true,"scan_id":"session_001","status":"running","projector_role":"projector","left_role":"left","right_role":"right","pattern_count":"44","captured_count":"0","current_index":"-1","output_dir":"./data/scan/session_001"}
```

`scan_start` は非同期に実行される。responseは開始受付完了を表し、実際の進捗はeventで通知される。`scan_id` 省略時は `scan_YYYYMMDD_HHMMSS` 形式で自動生成する。`settle_ms` 省略時は `120`。

scan workerは以下を順に実行する。

- ProjectorServiceでpatternをindex順に表示する。
- 表示後に `settle_ms` だけ待つ。
- left/right cameraでstereo captureする。
- `output_dir/left/pattern_000.png` と `output_dir/right/pattern_000.png` のように保存する。
- 全pattern完了時に `scan_completed` eventを出す。

出力directory構造:

```text
output_dir/
  metadata.json
  left/
    pattern_000.png
    pattern_001.png
  right/
    pattern_000.png
    pattern_001.png
```

`metadata.json` には `scan_id`, `created_at`, `version`, `projector_role`, `left_role`, `right_role`, `pattern_count`, `settle_ms`, `output_dir`, `surface` を保存する。GrayCode decode / reconstruct は今回行わない。scan中にsurface変更や `generate_patterns` は行わない。

scan中は、対象 `projector_role`、対象left/right camera role、対象projector window roleへの破壊的操作を拒否する。拒否されたcommandは `scan_resource_busy` を返す。

拒否される例:

- `close_projector`
- `generate_patterns`
- `configure_projector_surface`
- `show_pattern` / `next_pattern` / `prev_pattern`
- scan対象projector window roleへの `close_window` / `open_window`
- scan対象left/right roleへの `open_camera` / `close_camera`
- scan中のmanual capture / calibration capture command

scan workerもcaptureを実行するため、scan中のmanual captureはframe取得順や保存結果を壊さないようdispatcherで拒否する。これによりscan workerの `captureStereo` 中に対象cameraがremoveされることを防ぐ。CameraManager自体の汎用thread safety強化は別PRで扱う。

scan events:

```json
{"event":"scan_started","scan_id":"session_001","projector_role":"projector","left_role":"left","right_role":"right","pattern_count":"44","output_dir":"./data/scan/session_001"}
{"event":"scan_frame_captured","scan_id":"session_001","pattern_index":"0","captured_count":"1","pattern_count":"44","left_path":"./data/scan/session_001/left/pattern_000.png","right_path":"./data/scan/session_001/right/pattern_000.png"}
{"event":"scan_completed","scan_id":"session_001","captured_count":"44","pattern_count":"44","output_dir":"./data/scan/session_001"}
{"event":"scan_failed","scan_id":"session_001","error_code":"capture_failed","error_message":"failed to capture stereo frame","captured_count":"12","current_index":"12"}
{"event":"scan_stopped","scan_id":"session_001","captured_count":"12","current_index":"12"}
```

### scan_status

```json
{"id":"71","cmd":"scan_status"}
{"id":"71","ok":true,"scan_id":"session_001","status":"running","pattern_count":"44","captured_count":"12","current_index":"12","output_dir":"./data/scan/session_001"}
```

`scan_id` は任意。指定したscanが存在しない場合は `scan_not_found`。`scan_id` 未指定でscanが存在しない場合は `idle` を返す。

### scan_stop

```json
{"id":"72","cmd":"scan_stop","scan_id":"session_001"}
{"id":"72","ok":true,"scan_id":"session_001","status":"stopping"}
{"event":"scan_stopping","scan_id":"session_001"}
{"event":"scan_stopped","scan_id":"session_001","captured_count":"12","current_index":"12"}
```

`scan_stop` は実行中scanへ停止要求を出す。停止完了は worker から `scan_stopped` eventとして通知される。

### scan_validate

```json
{"id":"80","cmd":"scan_validate","input_dir":"./data/scan/session_001"}
{"id":"80","ok":true,"input_dir":"./data/scan/session_001","scan_id":"session_001","valid":"true","partial":"false","pattern_count":"44","left_count":"44","right_count":"44","missing_count":"0","issue_count":"0","width":"640","height":"480","issues_json":"[]"}
```

`allow_partial` は任意で、未指定時は `false`。

```json
{"id":"80","cmd":"scan_validate","input_dir":"./data/scan/session_001","allow_partial":false}
```

`scan_validate` は `scan_start` が保存したdatasetを、GrayCode decode / reconstructへ渡す前に検証する。検証処理自体が実行できた場合、datasetが不正でも原則 `ok=true`、`valid="false"` を返す。不備の詳細は `issues_json` にJSON array文字列として入る。

検証対象構造:

```text
output_dir/
  metadata.json
  left/
    pattern_000.png
  right/
    pattern_000.png
```

主な検証内容:

- `input_dir` が存在し、directoryであること。
- `metadata.json` が存在し、`scan_id`、正の `pattern_count`、正の `surface.pattern_width` / `surface.pattern_height` を持つこと。
- `left/` と `right/` が存在すること。
- `pattern_count` に対応する `left/right/pattern_NNN.png` が存在し、読み込み可能で、left/right sizeと全indexの画像sizeが整合すること。

`allow_partial=true` の場合、issueが `missing_left_image` / `missing_right_image` のみであれば `valid="true"` として扱える。metadata不備、directory不備、unreadable image、size mismatchは `allow_partial=true` でも `valid="false"`。

invalid dataset例:

```json
{"id":"80","ok":true,"input_dir":"./data/scan/session_001","scan_id":"session_001","valid":"false","partial":"true","pattern_count":"44","left_count":"40","right_count":"40","missing_count":"4","issue_count":"4","width":"640","height":"480","issues_json":"[{\"code\":\"missing_left_image\",\"message\":\"missing left image for pattern index 40\",\"path\":\"./data/scan/session_001/left/pattern_040.png\",\"pattern_index\":40}]"}
```

`input_dir` field自体が不足、または空文字の場合は mapper validation として `ok=false` を返す。GrayCode decode / reconstruct は行わない。

### decode_patterns

```json
{"id":"90","cmd":"decode_patterns","input_dir":"./data/scan/session_001","output_dir":"./data/decode/session_001","threshold":15}
{"id":"90","ok":true,"input_dir":"./data/scan/session_001","output_dir":"./data/decode/session_001","scan_id":"session_001","pattern_count":"44","image_width":"640","image_height":"480","projector_width":"1280","projector_height":"720","threshold":"15","left_valid_count":"245000","right_valid_count":"244300","left_valid_ratio":"0.7975","right_valid_ratio":"0.7952"}
```

`decode_patterns` は `scan_start` 出力datasetを入力にし、事前に `ScanDatasetValidator` でdatasetを検証してからGrayCode inverse pairをdecodeする。3D reconstructは行わない。

必須field:

- `input_dir`: scan dataset directory。
- `output_dir`: decode result output directory。

任意field:

- `threshold`: GrayCode inverse pairの明暗差threshold。未指定時は `15`。
- `allow_partial`: partial datasetを許可するか。未指定時は `false`。

mapper validation:

- `input_dir` 不足は `missing_field`。
- `output_dir` 不足は `missing_field`。
- `input_dir` / `output_dir` 空文字は `invalid_command`。
- `threshold < 0` は `invalid_command`。

出力構造:

```text
output_dir/
  metadata.json
  left/
    projector_x.yml
    projector_y.yml
    valid_mask.png
  right/
    projector_x.yml
    projector_y.yml
    valid_mask.png
```

保存形式:

- `projector_x.yml`: camera pixelごとのprojector active pattern x座標。`CV_32SC1`。invalid pixelは `-1`。
- `projector_y.yml`: camera pixelごとのprojector active pattern y座標。`CV_32SC1`。invalid pixelは `-1`。
- `valid_mask.png`: valid pixelは `255`、invalid pixelは `0`。`CV_8UC1`。

GrayCode pattern順序はOpenCV `GrayCodePattern::generate()` に合わせる。現在は `x` のMSBからLSBのnormal/inverse pair、続いて `y` のMSBからLSBのnormal/inverse pairをdecodeする。既存 `StructuredLight::generatePatterns()` は末尾に白/黒patternを追加するため、decodeではGrayCode部分を使い、末尾の白/黒は無視する。

threshold判定:

```text
abs(normal - inverse) < threshold なら invalid
normal > inverse なら gray bit = 1
normal <= inverse なら gray bit = 0
```

metadata例:

```json
{"scan_id":"session_001","input_dir":"./data/scan/session_001","output_dir":"./data/decode/session_001","pattern_count":44,"image_width":640,"image_height":480,"projector_width":1280,"projector_height":720,"threshold":15,"left_valid_count":245000,"right_valid_count":244300,"left_valid_ratio":0.7975,"right_valid_ratio":0.7952,"surface":{"surface_width":1920,"surface_height":1080,"pattern_width":1280,"pattern_height":720,"pattern_x":320,"pattern_y":180}}
```

`projector_x` / `projector_y` はcamera pixelからprojector active pattern座標へのmap。`metadata.surface.pattern_x` / `pattern_y` は後続reconstructでprojector full surface座標へ変換する際に利用できる。

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
| `pattern_show_failed` | WindowService経由のpattern表示失敗 |
| `pattern_index_out_of_range` | pattern indexが範囲外 |
| `pattern_generate_failed` | GrayCodePattern生成失敗 |
| `pattern_not_generated` | projector patternが未生成 |
| `projector_window_not_open` | projector表示先window_roleがopenされていない |
| `projector_window_configure_failed` | windowは存在するがprojector surface再設定に失敗 |
| `invalid_monitor_size` | monitor width/heightが0以下 |
| `projector_not_open` | roleにopen済みprojectorがない |
| `projector_already_open` | 指定projector_roleが既にopen済み |
| `invalid_projector_size` | projector width/heightが不正 |
| `invalid_projector_role` | projector_roleが空、または英数字、`_`、`-` 以外を含む |
| `capture_failed` | capture詳細errorがない失敗 |
| `scan_already_running` | scanが既にrunning/stopping |
| `scan_not_found` | 指定scan_idのscanが存在しない |
| `invalid_scan_config` | scan_start設定が不正 |
| `scan_start_failed` | scan開始準備に失敗 |
| `scan_failed` | scan workerが失敗 |
| `scan_stop_failed` | scan停止要求に失敗 |
| `scan_resource_busy` | scan中のprojector/camera/window roleへ干渉するcommandを拒否した |
| `input_dir_not_found` | scan_validate対象input_dirが存在しない |
| `input_dir_not_directory` | scan_validate対象input_dirがdirectoryではない |
| `metadata_not_found` | metadata.jsonが存在しない |
| `metadata_read_failed` | metadata.jsonを読み取れない |
| `metadata_parse_failed` | metadata.jsonをparseできない、またはobjectではない |
| `metadata_missing_scan_id` | metadata.jsonにscan_idがない |
| `metadata_invalid_pattern_count` | metadata.jsonのpattern_countが正の値ではない |
| `metadata_invalid_surface` | metadata.jsonのsurface pattern sizeが正の値ではない |
| `left_dir_not_found` | left/ directoryが存在しない |
| `right_dir_not_found` | right/ directoryが存在しない |
| `missing_left_image` | expected left imageが存在しない |
| `missing_right_image` | expected right imageが存在しない |
| `unreadable_left_image` | expected left imageを読み込めない |
| `unreadable_right_image` | expected right imageを読み込めない |
| `empty_left_image` | expected left imageがempty image |
| `empty_right_image` | expected right imageがempty image |
| `stereo_size_mismatch` | 同一indexのleft/right画像sizeが一致しない |
| `image_size_inconsistent` | pattern index間で画像sizeが一致しない |
| `scan_dataset_invalid` | decode_patterns入力datasetがdecode対象として不正 |
| `decode_pattern_count_mismatch` | metadata pattern_countがGrayCode decoderの期待枚数と一致しない |
| `decode_image_load_failed` | decode対象pattern画像の読み込みに失敗 |
| `decode_image_size_mismatch` | decode対象pattern画像のsizeが一致しない |
| `decode_output_write_failed` | decode結果fileの書き込みに失敗 |
| `decode_failed` | GrayCode decode処理中の予期しない失敗 |
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


Projector commandは次の経路で処理する。

```text
JSON Lines ControlMessage
  -> ControlInputAdapter
  -> HeadlessCommandMapper
  -> cmd::Projector系Command
  -> HeadlessDispatcher
  -> ProjectorHandler
  -> ProjectorService
  -> WindowService::showImage
  -> WindowManager
```

ProjectorServiceはWindowManagerを直接触らず、表示は `WindowService::showImage` 経由で行う。現段階ではwindow backendのみを利用し、DRM/KMSやHDMI直接制御は後続PRで扱う。

Scan dataset validation commandは次の経路で処理する。

```text
JSON Lines ControlMessage
  -> ControlInputAdapter
  -> HeadlessCommandMapper
  -> cmd::CmdValidateScanDataset
  -> HeadlessDispatcher
  -> ScanDatasetHandler
  -> ScanDatasetValidator
  -> common::CommandResult
  -> ControlResponse
```

`scan_validate` は保存済みdatasetだけを読むため、実行中scanのprojector/camera/window resourceには触らない。

Decode commandは次の経路で処理する。

```text
JSON Lines ControlMessage
  -> ControlInputAdapter
  -> HeadlessCommandMapper
  -> cmd::CmdDecodePatterns
  -> HeadlessDispatcher
  -> DecodeHandler
  -> DecodeService
  -> ScanDatasetValidator
  -> common::CommandResult
  -> ControlResponse
```

`decode_patterns` も保存済みdatasetを読むだけなので、実行中scan resourceには触らない。同じ `input_dir` がscan workerにより書き込み中かどうかは今回判定しない。

| JSONL `cmd` | C++ command | Handler | Service | 備考 |
| --- | --- | --- | --- | --- |
| `list_monitors` | `cmd::CmdListMonitors` | `handler::projector` | `service::monitor::MonitorService` | monitor一覧を返す |
| `configure_projector_surface` | `cmd::CmdConfigureProjectorSurface` | `handler::projector` | `service::projector::ProjectorService` | monitorに合わせてprojector surfaceとactive pattern areaを設定する |
| `open_projector` | `cmd::CmdOpenProjector` | `handler::projector` | `service::projector::ProjectorService` | projector_roleをwindow_roleへbindする |
| `close_projector` | `cmd::CmdCloseProjector` | `handler::projector` | `service::projector::ProjectorService` | projector bindingを解除する。windowは閉じない |
| `generate_patterns` | `cmd::CmdGeneratePatterns` | `handler::projector` | `service::projector::ProjectorService` | projector解像度でGrayCodePatternを生成する |
| `show_pattern` | `cmd::CmdProjectorShowPattern` | `handler::projector` | `service::projector::ProjectorService` | 指定indexのpatternをWindowService経由で表示する |
| `next_pattern` | `cmd::CmdProjectorNextPattern` | `handler::projector` | `service::projector::ProjectorService` | 次のpatternを表示する |
| `prev_pattern` | `cmd::CmdProjectorPrevPattern` | `handler::projector` | `service::projector::ProjectorService` | 前のpatternを表示する |
| `scan_validate` | `cmd::CmdValidateScanDataset` | `handler::scan_dataset` | `service::scan_dataset::ScanDatasetValidator` | scan_start出力datasetをdecode前に検証する |
| `decode_patterns` | `cmd::CmdDecodePatterns` | `handler::decode` | `service::decode::DecodeService` | scan datasetからprojector coordinate mapを生成する |
