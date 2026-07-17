# スキャンデータセット

scan dataset はGrayCode scanで取得した左右画像の集合である。metadata.jsonはscan_start生成datasetでは存在するが、手動保存datasetでは任意である。

## 構成

```text
scan_dataset/
  metadata.json
  left/
    pattern_000.png
  right/
    pattern_000.png
```

## 生成するcommand

- `scan_start`
- GUI scan

## 読むcommand

- `scan_validate`
- `decode_patterns`

## metadata field

| field | 説明 |
| --- | --- |
| `scan_id` | scan session ID。 |
| `created_at` | 生成時刻。 |
| `version` | metadata format version。 |
| `projector_role` | projector role。 |
| `left_role` | left camera role。 |
| `right_role` | right camera role。 |
| `pattern_count` | pattern画像数。 |
| `settle_ms` | pattern表示後に待つ時間。 |
| `output_dir` | dataset root。 |
| `surface.monitor_index` | monitor index。 |
| `surface.monitor_width` | monitor幅。 |
| `surface.monitor_height` | monitor高さ。 |
| `surface.surface_width` | surface幅。 |
| `surface.surface_height` | surface高さ。 |
| `surface.pattern_width` | active pattern幅。 |
| `surface.pattern_height` | active pattern高さ。 |
| `surface.pattern_x` | active pattern X座標。 |
| `surface.pattern_y` | active pattern Y座標。 |
| `surface.clamped` | active pattern areaがmonitor内へclampされたか。 |

## metadata任意field

| field | 説明 |
| --- | --- |
| `notes` | 実験メモ。 |
| `operator` | 実行者。 |

## file format

`metadata.json` はJSONである。
pattern画像はPNGであり、`pattern_NNN.png` 形式で読む。
metadataが無い場合、`scan_validate` と `decode_patterns` はleft/right directoryからpattern数を推定する。
left/rightのpattern indexは一致する。
