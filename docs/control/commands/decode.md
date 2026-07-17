# デコードcommand

## decode_patterns

### 役割

decode_patterns はファイル処理commandである。
保存済みscan datasetだけを読む。
カメラ、Window、プロジェクタ表示、scan processには依存しない。
metadata.jsonが無い場合は、`projector_width` / `projector_height` を指定してdecodeできる。

### args(JSONL)

```json
{"id":"90","cmd":"decode_patterns","input_dir":"./data/scan/session_001","output_dir":"./data/decode/session_001","threshold":10,"allow_partial":false}
```

```json
{"id":"90","cmd":"decode_patterns","left_dir":"./captures/scan_L","right_dir":"./captures/scan_R","output_dir":"./data/decode/manual_001","projector_width":1280,"projector_height":720,"threshold":10,"allow_partial":false}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `decode_patterns`。 |
| `input_dir` | 条件付き必須 | scan dataset root、left/rightを含む親directory、または`left`/`right` directoryそのもの。末尾スラッシュ付きpathも同じ意味で扱う。`left_dir`/`right_dir`指定時は省略可。 |
| `output_dir` | 必須 | decode result出力directory。 |
| `left_dir` | 条件付き必須 | left画像directory。`input_dir`指定時は省略可。 |
| `right_dir` | 条件付き必須 | right画像directory。`input_dir`指定時は省略可。 |
| `metadata_file` | 任意 | 明示metadata file。省略時はrootの`metadata.json`を探す。 |
| `threshold` | 任意 | GrayCode decodeの閾値。 |
| `allow_partial` | 任意 | 欠損画像を許可するか。 |
| `projector_width` | metadataなし時は必須 | metadataがない場合のprojector幅。 |
| `projector_height` | metadataなし時は必須 | metadataがない場合のprojector高さ。 |
| `pattern_count` | 任意 | metadataがない場合のpattern数。省略時は`pattern_*.png`から推定する。 |

### return

```json
{"id":"90","ok":true,"input_dir":"./data/scan/session_001","output_dir":"./data/decode/session_001","scan_id":"session_001","pattern_count":"44","image_width":"1280","image_height":"720","projector_width":"1920","projector_height":"1080","threshold":"10","left_valid_count":"123456","right_valid_count":"123120","left_valid_ratio":"0.1340","right_valid_ratio":"0.1336"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | command処理が完了したか。 |
| `input_dir` | scan dataset root。 |
| `output_dir` | decode result出力directory。 |
| `scan_id` | scan session ID。 |
| `pattern_count` | decodeしたpattern数。 |
| `image_width` | capture画像幅。 |
| `image_height` | capture画像高さ。 |
| `projector_width` | projector pattern幅。 |
| `projector_height` | projector pattern高さ。 |
| `threshold` | decodeに使用した閾値。 |
| `left_valid_count` | left decodeの有効pixel数。 |
| `right_valid_count` | right decodeの有効pixel数。 |
| `left_valid_ratio` | left decodeの有効pixel率。小数第4位までの文字列。 |
| `right_valid_ratio` | right decodeの有効pixel率。小数第4位までの文字列。 |

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
| `invalid_command` | `input_dir`、`output_dir`、`threshold` が不正である。 |
| `scan_dataset_invalid` | scan dataset検証に失敗した。入力directory欠落、画像欠損、metadataなしでprojector size未指定などを含む。 |
| `decode_pattern_count_mismatch` | decodeに必要なpattern数とdatasetのpattern数が一致しない。 |
| `decode_image_load_failed` | pattern画像の読み込みに失敗した。 |
| `decode_image_size_mismatch` | pattern画像のsizeが一致しない、またはleft/right decoded image sizeが一致しない。 |
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
