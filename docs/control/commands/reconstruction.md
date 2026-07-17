# 3D再構成command

## reconstruct_point_cloud

### 役割

decode resultとstereo calibration fileからPLY point cloudを生成する。
camera、projector、scan processには依存しない。
現在は未実装である。

### args(JSONL)

```json
{"id":"100","cmd":"reconstruct_point_cloud","decode_dir":"./data/decode/session_001","calibration_file":"./data/calib/stereo.yml","output_file":"./data/reconstruction/session_001/cloud.ply"}
```

| field | 必須 | 説明 |
| --- | --- | --- |
| `id` | 必須 | request ID。 |
| `cmd` | 必須 | `reconstruct_point_cloud`。 |
| `decode_dir` | 必須 | decode result directory。 |
| `calibration_file` | 必須 | stereo calibration file。 |
| `output_file` | 必須 | PLY出力file。 |
| `max_points` | 任意 | 出力点数の上限。 |
| `min_disparity` | 任意 | 使用する最小disparity。 |
| `max_reprojection_error` | 任意 | 許容する最大reprojection error。 |

### return

```json
{"id":"100","ok":true,"output_file":"./data/reconstruction/session_001/cloud.ply","point_count":"12345"}
```

| field | 説明 |
| --- | --- |
| `id` | request ID。 |
| `ok` | command処理が完了したか。 |
| `output_file` | 書き出したPLY file。 |
| `point_count` | 出力点数。 |

### event

なし。

### 読むArtifact

decode result。
stereo calibration file。

### 書くArtifact

PLY point cloud。

### 必要なruntime resource

なし。

### error code

| code | 条件 |
| --- | --- |
| `command_not_implemented` | 現在は未実装である。 |
| `missing_field` | 必須fieldがない。 |
| `decode_dir_not_found` | decode resultが存在しない。 |
| `calibration_file_not_found` | stereo calibration fileが存在しない。 |
| `reconstruction_failed` | 3D再構成に失敗した。 |
| `reconstruction_output_write_failed` | PLY fileを書けない。 |
