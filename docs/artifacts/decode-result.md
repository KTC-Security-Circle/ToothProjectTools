# デコード結果

decode result はscan datasetから計算したprojector座標とmaskの集合である。

## 構成

```text
decode_result/
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

## 生成するcommand

- `decode_patterns`

## 読むcommand

- `reconstruct_point_cloud`

## 必須field

| field | 説明 |
| --- | --- |
| `metadata.version` | decode result format version。 |
| `metadata.input_dir` | 元のscan dataset root。 |
| `metadata.pattern_count` | decodeしたpattern数。 |
| `metadata.projector_width` | projector座標の幅。 |
| `metadata.projector_height` | projector座標の高さ。 |
| `left/projector_x.yml` | left画像各pixelのprojector X座標。 |
| `left/projector_y.yml` | left画像各pixelのprojector Y座標。 |
| `left/valid_mask.png` | left decodeの有効mask。 |
| `right/projector_x.yml` | right画像各pixelのprojector X座標。 |
| `right/projector_y.yml` | right画像各pixelのprojector Y座標。 |
| `right/valid_mask.png` | right decodeの有効mask。 |

## 任意field

| field | 説明 |
| --- | --- |
| `metadata.threshold` | decode時の閾値。 |
| `metadata.allow_partial` | 欠損画像を許可したか。 |

## file format

`metadata.json` はJSONである。
`projector_x.yml` と `projector_y.yml` はOpenCV FileStorage YAMLである。
`valid_mask.png` は8-bit PNGである。
