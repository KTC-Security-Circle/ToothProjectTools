# ウィンドウとプロジェクタcommand

## open_window

### 役割

Windowを作成し、window roleへbindする。

`monitor_index` は0-basedで、省略時はprimary monitorを使用する。範囲外はprimary monitorへfallbackし、monitorが存在しない場合は `monitor_not_found` を返す。既存schemaとの互換性のため、open response/eventには `monitor_index` を追加しない。

### args(JSONL)

```json
{"id":"40","cmd":"open_window","window_role":"projector","title":"Projector","width":1920,"height":1080,"monitor_index":0,"fullscreen":true}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `open_window`。 |
| `window_role` | 必須 | window role。 |
| `title` | 任意 | window title。省略時は `window_role`。 |
| `width` | 必須 | window幅。 |
| `height` | 必須 | window高さ。 |
| `monitor_index` | 任意 | 0-based monitor index。省略時はprimary、範囲外はprimaryへfallbackする。 |
| `fullscreen` | 任意 | fullscreen指定。 |

### return

```json
{"id":"40","ok":true,"window_role":"projector","window_id":"1","width":"1920","height":"1080"}
```

### event

```json
{"event":"window_opened","window_role":"projector","window_id":"1","width":"1920","height":"1080"}
```

### サイズの意味

- `code_width` / `code_height`: Gray Code論理解像度。`cv::structured_light::GrayCodePattern` へ渡す解像度。
- `surface_width` / `surface_height`: monitor全体を覆うblack canvasのサイズ。
- `display_width` / `display_height`: surface内でpatternを表示する矩形サイズ。互換fieldの `pattern_width` / `pattern_height` も同じdisplay regionを表す。
- 表示時は論理patternをdisplay regionへnearest-neighborで拡大する。

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

Window backend。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `invalid_command` | sizeまたはmonitor_indexが不正である。 |
| `window_open_failed` | Windowを作成できない。 |
| `window_already_open` | roleが既にopen済みである。 |
| `scan_resource_busy` | scanが対象window roleを使用中である。 |

## close_window

### 役割

window roleにbindされたWindowをcloseする。

### args(JSONL)

```json
{"id":"41","cmd":"close_window","window_role":"projector"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `close_window`。 |
| `window_role` | 必須 | window role。 |

### return

```json
{"id":"41","ok":true,"window_role":"projector","window_id":"1"}
```

### event

```json
{"event":"window_closed","window_role":"projector","window_id":"1"}
```

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

open済みwindow role。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `window_role` がない。 |
| `window_not_open` | roleがopenされていない。 |
| `window_close_failed` | closeに失敗した。 |
| `scan_resource_busy` | scanが対象window roleを使用中である。 |

## list_monitors

### 役割

monitor一覧を返す。
monitor indexは0-basedである。monitorを取得できない場合は空の一覧を返す。

### args(JSONL)

```json
{"id":"60","cmd":"list_monitors"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `list_monitors`。 |

### return

```json
{"id":"60","ok":true,"monitor_count":"1","monitors_json":"[{"monitor_index":0,"x":0,"y":0,"width":1920,"height":1080,"primary":true,"name":"default","fallback":true}]"}
```

### event

なし。

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

monitor provider。

### error code

| code | 条件 |
| --- | --- |
| `internal_error` | monitor取得処理で未処理例外が発生した。 |

## configure_projector_surface

### 役割

projectorのmonitor surfaceと、Gray Codeを実際に表示するdisplay regionを設定する。

monitorが1台以上ある場合、範囲外の明示指定はprimary monitorへfallbackする。monitorが存在しない場合は `monitor_not_found` を返す。responseとeventの `monitor_index` はrequest値ではなく実際に適用された0-based indexである。

### args(JSONL)

```json
{"id":"61","cmd":"configure_projector_surface","projector_role":"projector","monitor_index":0,"width":1280,"height":720,"placement":"center"}
```

```json
{"id":"62","cmd":"configure_projector_surface","projector_role":"projector","monitor_index":0,"width":1280,"height":720,"x":320,"y":180,"placement":"custom"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `configure_projector_surface`。 |
| `projector_role` | 必須 | projector role。 |
| `monitor_index` | 必須 | 0-based monitor index。範囲外はprimaryへfallbackする。 |
| `width` | 必須 | monitor上のdisplay region幅。Gray Code論理解像度は変更しない。 |
| `height` | 必須 | monitor上のdisplay region高さ。Gray Code論理解像度は変更しない。 |
| `placement` | 任意 | `center` または `custom`。defaultは `center`。 |
| `x` | 条件付き | `custom` のX座標。 |
| `y` | 条件付き | `custom` のY座標。 |

### return

```json
{"id":"61","ok":true,"projector_role":"projector","window_role":"projector","monitor_index":"0","monitor_width":"1920","monitor_height":"1080","surface_width":"1920","surface_height":"1080","code_width":"960","code_height":"540","pattern_width":"1280","pattern_height":"720","pattern_x":"320","pattern_y":"180","display_width":"1280","display_height":"720","display_x":"320","display_y":"180","clamped":"false"}
```

### event

```json
{"event":"projector_surface_configured","projector_role":"projector","window_role":"projector","monitor_index":"0","code_width":"960","code_height":"540","pattern_width":"1280","pattern_height":"720","pattern_x":"320","pattern_y":"180","display_width":"1280","display_height":"720","display_x":"320","display_y":"180","clamped":"false"}
```

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

open済みprojector role。
monitor provider。
Window backend。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `invalid_command` | size、monitor_index、placementが不正である。 |
| `projector_not_open` | projector roleがopenされていない。 |
| `projector_window_configure_failed` | Windowの移動またはresizeに失敗した。 |
| `scan_resource_busy` | scanが対象projector roleを使用中である。 |

## open_projector

### 役割

projector roleをopen済みwindow roleへbindする。
ここでのprojectorは物理デバイスではなく、patternを表示するruntime上の表示resourceを指す。

初期surfaceはprimary monitorを使用する。`width` / `height` は Gray Code 論理解像度として保存される。monitorが存在しない場合は `monitor_not_found` を返し、projector stateを作成しない。response/eventにsurfaceの `monitor_index` が含まれる場合は実際に適用された0-based indexである。

### args(JSONL)

```json
{"id":"50","cmd":"open_projector","projector_role":"projector","window_role":"projector","width":1920,"height":1080}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `open_projector`。 |
| `projector_role` | 必須 | projector role。 |
| `window_role` | 必須 | window role。 |
| `width` | 必須 | Gray Code論理解像度の幅。 |
| `height` | 必須 | Gray Code論理解像度の高さ。 |

### return

```json
{"id":"50","ok":true,"projector_role":"projector","window_role":"projector","width":"1920","height":"1080","code_width":"1920","code_height":"1080","surface_width":"1920","surface_height":"1080","pattern_width":"1920","pattern_height":"1080","display_width":"1920","display_height":"1080","pattern_x":"0","pattern_y":"0","display_x":"0","display_y":"0","clamped":"false"}
```

### event

```json
{"event":"projector_opened","projector_role":"projector","window_role":"projector","width":"1920","height":"1080","code_width":"1920","code_height":"1080"}
```

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

open済みwindow role。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `invalid_command` | sizeが不正である。 |
| `projector_window_not_open` | window roleがopenされていない。 |
| `projector_already_open` | projector roleが既にopen済みである。 |
| `scan_resource_busy` | scanが対象resourceを使用中である。 |

## close_projector

### 役割

projector bindingを解除する。
Windowはcloseしない。

### args(JSONL)

```json
{"id":"55","cmd":"close_projector","projector_role":"projector"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `close_projector`。 |
| `projector_role` | 必須 | projector role。 |

### return

```json
{"id":"55","ok":true,"projector_role":"projector"}
```

### event

```json
{"event":"projector_closed","projector_role":"projector"}
```

### 読むArtifact

なし。

### 書くArtifact

なし。

### 必要なruntime resource

open済みprojector role。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `projector_role` がない。 |
| `projector_not_open` | projector roleがopenされていない。 |
| `scan_resource_busy` | scanが対象projector roleを使用中である。 |

## generate_patterns

### 役割

generate_patterns はpattern生成commandである。
カメラや物理プロジェクタには依存しない。
生成するpatternの幅・高さは `open_projector` で保存した Gray Code論理解像度に依存する。`configure_projector_surface` のdisplay region幅・高さは生成解像度を変更しない。

### args(JSONL)

```json
{"id":"51","cmd":"generate_patterns","projector_role":"projector"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `generate_patterns`。 |
| `projector_role` | 必須 | projector role。 |

### return

```json
{"id":"51","ok":true,"projector_role":"projector","pattern_count":"42","width":"960","height":"540","code_width":"960","code_height":"540","display_width":"1920","display_height":"1080"}
```

### event

```json
{"event":"patterns_generated","projector_role":"projector","pattern_count":"42","width":"960","height":"540","code_width":"960","code_height":"540","display_width":"1920","display_height":"1080"}
```

### 読むArtifact

なし。

### 書くArtifact

runtime上のprojector pattern。
filesystem artifactは書かない。

### 必要なruntime resource

open済みprojector role。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `projector_role` がない。 |
| `projector_not_open` | projector roleがopenされていない。 |
| `pattern_generate_failed` | pattern生成に失敗した。 |
| `scan_resource_busy` | scanが対象projector roleを使用中である。 |

## show_pattern

### 役割

show_pattern はプロジェクタ表示commandである。
ここでのプロジェクタは物理デバイスではなく、patternを表示するruntime上の表示resourceを指す。
指定indexのpatternをWindowへ表示する。

### args(JSONL)

```json
{"id":"52","cmd":"show_pattern","projector_role":"projector","index":0,"photodiode_marker_mode":"sync"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `show_pattern`。 |
| `projector_role` | 必須 | projector role。 |
| `index` | 必須 | pattern index。 |
| `photodiode_marker_mode` | 任意 | `sync`（偶数black、奇数white、32x32）または `locate`（赤、96/64/32px）。省略時は現在のmodeを維持する。 |

### return

```json
{"id":"52","ok":true,"projector_role":"projector","pattern_index":"0","photodiode_marker_mode":"sync","marker_x":"944","marker_y":"534","marker_width":"32","marker_height":"32"}
```

### event

```json
{"event":"pattern_shown","projector_role":"projector","pattern_index":"0"}
```

### 読むArtifact

runtime上のprojector pattern。

### 書くArtifact

なし。

### 必要なruntime resource

open済みprojector role。
Window backend。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `invalid_command` | indexが負である。 |
| `pattern_not_generated` | patternが生成されていない。 |
| `pattern_index_out_of_range` | indexが範囲外である。 |
| `pattern_show_failed` | 表示に失敗した。 |
| `scan_resource_busy` | scanが対象projector roleを使用中である。 |

## next_pattern

### 役割

next_pattern はプロジェクタ表示commandである。
次のpatternを表示する。

### args(JSONL)

```json
{"id":"53","cmd":"next_pattern","projector_role":"projector"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `next_pattern`。 |
| `projector_role` | 必須 | projector role。 |

### return

```json
{"id":"53","ok":true,"projector_role":"projector","pattern_index":"1"}
```

### event

```json
{"event":"pattern_shown","projector_role":"projector","pattern_index":"1"}
```

### 読むArtifact

runtime上のprojector pattern。

### 書くArtifact

なし。

### 必要なruntime resource

open済みprojector role。
Window backend。

### error code

`show_pattern` と同じである。

## prev_pattern

### 役割

prev_pattern はプロジェクタ表示commandである。
前のpatternを表示する。

### args(JSONL)

```json
{"id":"54","cmd":"prev_pattern","projector_role":"projector"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `prev_pattern`。 |
| `projector_role` | 必須 | projector role。 |

### return

```json
{"id":"54","ok":true,"projector_role":"projector","pattern_index":"0"}
```

### event

```json
{"event":"pattern_shown","projector_role":"projector","pattern_index":"0"}
```

### 読むArtifact

runtime上のprojector pattern。

### 書くArtifact

なし。

### 必要なruntime resource

open済みprojector role。
Window backend。

### error code

`show_pattern` と同じである。
