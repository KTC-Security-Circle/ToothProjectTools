# コマンド一覧

| command | 分類 | args(JSONL) | return | event | 読むArtifact | 書くArtifact | 必要なruntime resource | 実装状態 | 詳細docs |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `ping` | プロトコル制御 | `id`, `cmd` | `result` | なし | なし | なし | serve app | 実装済み | [protocol.md](./protocol.md#ping) |
| `shutdown` | プロトコル制御 | `id`, `cmd` | `ok` | scan残event | なし | なし | serve app | 実装済み | [protocol.md](./protocol.md#shutdown) |
| `open_camera` | カメラ操作 | `id`, `cmd`, `camera_id`, `role` | `ok` | `camera_opened` | なし | なし | camera device | 実装済み | [camera.md](./commands/camera.md#open_camera) |
| `close_camera` | カメラ操作 | `id`, `cmd`, `role` | `ok` | なし | なし | なし | camera role | 実装済み | [camera.md](./commands/camera.md#close_camera) |
| `start_stream` | カメラ操作 | `id`, `cmd`, `role` | `url` | `stream_started` | なし | なし | open済みcamera role、MJPEG server | 実装済み | [stream.md](./commands/stream.md#start_stream) |
| `stop_stream` | カメラ操作 | `id`, `cmd`, `role` | `ok` | なし | なし | なし | stream publisher | 実装済み | [stream.md](./commands/stream.md#stop_stream) |
| `capture_frame` | 撮影 | `id`, `cmd`, `role`, `output` | `path` | `frame_saved` | なし | image file | open済みcamera role | 実装済み | [capture.md](./commands/capture.md#capture_frame) |
| `capture_stereo` | 撮影 | `id`, `cmd`, `left_role`, `right_role`, `left_output`, `right_output` | `left_path`, `right_path` | `stereo_frame_saved` | なし | image files | open済みcamera roles | 実装済み | [capture.md](./commands/capture.md#capture_stereo) |
| `calib_capture_frame` | 撮影 | `id`, `cmd`, `role`, `output` | `path`, `purpose` | `calibration_frame_saved` | なし | calibration image | open済みcamera role | 実装済み | [capture.md](./commands/capture.md#calib_capture_frame) |
| `calib_capture_stereo` | 撮影 | `id`, `cmd`, `left_role`, `right_role`, `left_output`, `right_output` | `left_path`, `right_path`, `purpose` | `calibration_stereo_frame_saved` | なし | calibration images | open済みcamera roles | 実装済み | [capture.md](./commands/capture.md#calib_capture_stereo) |
| `calib_detect_corners` | 校正preview | `id`, `cmd`, `role`, `output`, board config | `role`, `found`, `corner_count`, `expected_corner_count`, `path` | なし | camera frame | preview image | open済みcamera role | 実装済み | [calibration.md](./commands/calibration.md#calib_detect_corners) |
| `calib_detect_stereo_corners` | 校正preview | `id`, `cmd`, `left_role`, `right_role`, `left_output`, `right_output`, board config | detection fields, paths | なし | camera frames | preview images | open済みcamera roles | 実装済み | [calibration.md](./commands/calibration.md#calib_detect_stereo_corners) |
| `mono_calibrate` | ファイル処理 | `id`, `cmd`, `role`, `image_folder`, `output_file`, board config | `role`, `output_file`, `rms` | `mono_calibration_finished` | calibration images | mono calibration file | 現在はopen済みcamera role | 一部実装 | [calibration.md](./commands/calibration.md#mono_calibrate) |
| `stereo_calibrate` | ファイル処理 | `id`, `cmd`, `left_role`, `right_role`, `left_dir`, `right_dir`, `output_file`, board config | `left_role`, `right_role`, `output_file`, `rms` | `stereo_calibration_finished` | calibration images | stereo calibration file | 現在はopen済みcamera roles | 一部実装 | [calibration.md](./commands/calibration.md#stereo_calibrate) |
| `open_window` | Window操作 | `id`, `cmd`, `window_role`, `width`, `height` | `window_role`, `window_id`, `width`, `height` | `window_opened` | なし | なし | Window backend | 実装済み | [projector.md](./commands/projector.md#open_window) |
| `close_window` | Window操作 | `id`, `cmd`, `window_role` | `window_role`, `window_id` | `window_closed` | なし | なし | open済みwindow role | 実装済み | [projector.md](./commands/projector.md#close_window) |
| `list_monitors` | Window操作 | `id`, `cmd` | `monitor_count`, `monitors_json` | なし | なし | なし | monitor provider | 実装済み | [projector.md](./commands/projector.md#list_monitors) |
| `configure_projector_surface` | プロジェクタ表示 | `id`, `cmd`, `projector_role`, `monitor_index`, `width`, `height` | surface fields | `projector_surface_configured` | なし | なし | open済みprojector role、monitor | 実装済み | [projector.md](./commands/projector.md#configure_projector_surface) |
| `open_projector` | プロジェクタ表示 | `id`, `cmd`, `projector_role`, `window_role`, `width`, `height` | projector fields | `projector_opened` | なし | なし | open済みwindow role | 実装済み | [projector.md](./commands/projector.md#open_projector) |
| `close_projector` | プロジェクタ表示 | `id`, `cmd`, `projector_role` | `projector_role` | `projector_closed` | なし | なし | open済みprojector role | 実装済み | [projector.md](./commands/projector.md#close_projector) |
| `generate_patterns` | pattern生成 | `id`, `cmd`, `projector_role` | `pattern_count`, `width`, `height` | `patterns_generated` | なし | runtime pattern | open済みprojector role | 実装済み | [projector.md](./commands/projector.md#generate_patterns) |
| `show_pattern` | プロジェクタ表示 | `id`, `cmd`, `projector_role`, `index` | `pattern_index` | `pattern_shown` | runtime pattern | なし | open済みprojector role、Window | 実装済み | [projector.md](./commands/projector.md#show_pattern) |
| `next_pattern` | プロジェクタ表示 | `id`, `cmd`, `projector_role` | `pattern_index` | `pattern_shown` | runtime pattern | なし | open済みprojector role、Window | 実装済み | [projector.md](./commands/projector.md#next_pattern) |
| `prev_pattern` | プロジェクタ表示 | `id`, `cmd`, `projector_role` | `pattern_index` | `pattern_shown` | runtime pattern | なし | open済みprojector role、Window | 実装済み | [projector.md](./commands/projector.md#prev_pattern) |
| `scan_start` | 状態管理 | `id`, `cmd`, `projector_role`, `left_role`, `right_role`, `output_dir` | scan status | scan events | runtime pattern | scan dataset | camera roles、projector role、Window | 実装済み | [scan.md](./commands/scan.md#scan_start) |
| `scan_status` | 状態管理 | `id`, `cmd` | scan status | なし | なし | なし | ScanService state | 実装済み | [scan.md](./commands/scan.md#scan_status) |
| `scan_stop` | 状態管理 | `id`, `cmd` | scan status | `scan_stopping`, `scan_stopped` | なし | なし | ScanService worker | 実装済み | [scan.md](./commands/scan.md#scan_stop) |
| `scan_validate` | ファイル処理 | `id`, `cmd`, `input_dir`, `allow_partial` | validation fields | なし | scan dataset | なし | なし | 実装済み | [scan-dataset.md](./commands/scan-dataset.md#scan_validate) |
| `decode_patterns` | ファイル処理 | `id`, `cmd`, `input_dir`, `output_dir`, `threshold` | decode fields | なし | scan dataset | decode result | なし | 実装済み | [decode.md](./commands/decode.md#decode_patterns) |
| `reconstruct_validate` | ファイル処理 | `id`, `cmd`, `decode_dir`, `calibration_file` | validation fields | なし | decode result、stereo calibration file | なし | なし | 実装済み | [reconstruction.md](./commands/reconstruction.md#reconstruct_validate) |
| `reconstruct_point_cloud` | ファイル処理 | `id`, `cmd`, `decode_dir`, `calibration_file`, `output_file` | `output_file`, `point_count` | なし | decode result、stereo calibration file | PLY file | なし | 実装済み | [reconstruction.md](./commands/reconstruction.md#reconstruct_point_cloud) |
