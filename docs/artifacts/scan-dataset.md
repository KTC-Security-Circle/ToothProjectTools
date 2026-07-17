# スキャンデータセット

scan dataset はGrayCode scanで取得した左右画像の集合である。

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

## 必須field

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

## 任意field

| field | 説明 |
| --- | --- |
| `notes` | 実験メモ。 |
| `operator` | 実行者。 |

## file format

`metadata.json` はJSONである。
pattern画像はPNGである。
left/rightのpattern indexは一致する。
