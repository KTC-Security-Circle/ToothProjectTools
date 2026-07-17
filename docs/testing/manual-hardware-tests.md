# 手動ハードウェアテスト

## 実行条件

手動ハードウェアテストはCIで実行しない。
実カメラ、Window表示、projector surface設定が必要である。

## 実カメラが必要なテスト

- `open_camera`
- `close_camera`
- `capture_frame`
- `capture_stereo`
- `calib_capture_frame`
- `calib_capture_stereo`
- `start_stream`
- `stop_stream`

## 実Window表示が必要なテスト

- `open_window`
- `close_window`
- `list_monitors`
- `configure_projector_surface`

## projector surface設定が必要なテスト

- `open_projector`
- `close_projector`
- `generate_patterns`
- `show_pattern`
- `next_pattern`
- `prev_pattern`

## scan_startを使うテスト

`scan_start` はleft camera、right camera、projector、patternを使う。
returnは受付結果だけを確認する。
進捗、完了、失敗はeventで確認する。

## CIで実行しない理由

実行結果が接続機器、monitor配置、projector surface、照明条件に依存するためである。
