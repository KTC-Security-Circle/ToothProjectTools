# スキャンデータセットcommand

## scan_validate

### 役割

保存済みscan datasetを検証する。
カメラ、Window、プロジェクタ表示、scan processには依存しない。

### args(JSONL)

```json
{"id":"80","cmd":"scan_validate","input_dir":"./data/scan/session_001","allow_partial":false}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `scan_validate`。 |
| `input_dir` | 現在は必須 | scan dataset root。 |
| `left_dir` | 未対応 | left画像directory。 |
| `right_dir` | 未対応 | right画像directory。 |
| `allow_partial` | 任意 | 欠損画像を許可するか。 |

現在の実装では `input_dir` はdataset rootだけを受け付ける。
`left_dir` / `right_dir` 明示指定は未対応である。

### return

```json
{"id":"80","ok":true,"input_dir":"./data/scan/session_001","scan_id":"session_001","valid":"true","partial":"false","pattern_count":"44","left_count":"44","right_count":"44","missing_count":"0","issue_count":"0","width":"1280","height":"720","issues_json":"[]"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | command処理が完了したか。 |
| `input_dir` | 検証対象directory。 |
| `scan_id` | metadataから読んだscan session ID。 |
| `valid` | left/right画像ペアとして成立するか。 |
| `partial` | 欠損画像があるが一部は存在する状態か。 |
| `pattern_count` | pattern数。 |
| `left_count` | left画像数。 |
| `right_count` | right画像数。 |
| `missing_count` | 欠損画像数。 |
| `issue_count` | 検証issue数。 |
| `width` | capture画像幅。未確定なら `0`。 |
| `height` | capture画像高さ。未確定なら `0`。 |
| `issues_json` | issue一覧JSONを文字列化した値。 |

### event

なし。

### 読むArtifact

scan dataset。

### 書くArtifact

なし。

### 必要なruntime resource

なし。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | `input_dir` がない。 |
| `input_dir_not_found` | 入力directoryが存在しない。 |
| `invalid_command` | field値が不正である。 |

## 判定値

| field | 定義 |
| --- | --- |
| `valid` | left/right画像ペアとして成立している。 |
| `partial` | 欠損画像があるが一部は存在する状態である。 |
