# デコードcommand

## decode_patterns

### 役割

decode_patterns はファイル処理commandである。
保存済みscan datasetだけを読む。
カメラ、Window、プロジェクタ表示、scan processには依存しない。
現在の実装では `metadata.json` が必須である。

### args(JSONL)

```json
{"id":"90","cmd":"decode_patterns","input_dir":"./data/scan/session_001","output_dir":"./data/decode/session_001","threshold":10,"allow_partial":false}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `decode_patterns`。 |
| `input_dir` | 必須 | scan dataset root。 |
| `output_dir` | 必須 | decode result出力directory。 |
| `threshold` | 任意 | GrayCode decodeの閾値。 |
| `allow_partial` | 任意 | 欠損画像を許可するか。 |
| `projector_width` | 未対応 | metadataがない場合のprojector幅。 |
| `projector_height` | 未対応 | metadataがない場合のprojector高さ。 |

### return

```json
{"id":"90","ok":true,"input_dir":"./data/scan/session_001","output_dir":"./data/decode/session_001","pattern_count":"44","left_valid_pixels":"123456","right_valid_pixels":"123120"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | command処理が完了したか。 |
| `input_dir` | scan dataset root。 |
| `output_dir` | decode result出力directory。 |
| `pattern_count` | decodeしたpattern数。 |
| `left_valid_pixels` | left decodeの有効pixel数。 |
| `right_valid_pixels` | right decodeの有効pixel数。 |

### event

なし。

### 読むArtifact

scan dataset。

### 書くArtifact

decode result。

### 必要なruntime resource

なし。

### error code

| code | 条件 |
| --- | --- |
| `missing_field` | 必須fieldがない。 |
| `input_dir_not_found` | scan datasetが存在しない。 |
| `metadata_missing` | metadata.jsonが存在しない。 |
| `decode_not_ready` | decodeに必要なmetadataが不足している。 |
| `decode_failed` | GrayCode decodeに失敗した。 |
| `decode_output_write_failed` | decode resultを書けない。 |

## 出力構成

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
