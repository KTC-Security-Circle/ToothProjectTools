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
| `pattern_count` | Gray Code論理解像度から生成したpattern画像数。 |
| `projector_width` | Decode用Gray Code論理解像度の幅。 |
| `projector_height` | Decode用Gray Code論理解像度の高さ。 |
| `sync` | `photodiode`。 |
| `photodiode_device` | scan時のserial device path。 |
| `photodiode_baud` | serial baud。 |
| `sync_timeout_ms` | event/frame timeout。 |
| `sync_guard_ms` | eventからframe selectionまでのguard。 |
| `output_dir` | dataset root。 |
| `surface.monitor_index` | monitor index。 |
| `surface.monitor_width` | monitor幅。 |
| `surface.monitor_height` | monitor高さ。 |
| `surface.surface_width` | surface幅。 |
| `surface.surface_height` | surface高さ。 |
| `surface.pattern_width` | display region幅。互換field。 |
| `surface.display_width` | display region幅。 |
| `surface.pattern_height` | display region高さ。互換field。 |
| `surface.display_height` | display region高さ。 |
| `surface.pattern_x` | display region X座標。互換field。 |
| `surface.display_x` | display region X座標。 |
| `surface.pattern_y` | display region Y座標。互換field。 |
| `surface.display_y` | display region Y座標。 |
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
