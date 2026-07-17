# リソースの所有

## 所有者

| resource | 所有者 | 操作する実装 |
| --- | --- | --- |
| Camera device | C++ sidecar | `CameraManager`, `CameraService` |
| Camera frame | C++ sidecar | `CaptureService`, stream処理 |
| MJPEG publisher | C++ sidecar | `SidecarService`, stream registry |
| Window | C++ sidecar | `WindowService` |
| Projector pattern | C++ sidecar | `ProjectorService` |
| Scan worker | C++ sidecar | `ScanService` |
| Calibration file | filesystem | `mono_calibrate`, `stereo_calibrate` |
| Scan dataset | filesystem | `scan_start`, `scan_validate`, `decode_patterns` |
| Decode result | filesystem | `decode_patterns`, `reconstruct_point_cloud` |
| Point cloud | filesystem | `reconstruct_point_cloud`。現在は未実装。 |

## ルール

- Tauri / Rust はcamera deviceを直接openしない。
- C++ sidecarがcameraを所有する。
- scan中は対象cameraへの破壊的操作を拒否する。
- scan中は対象projectorへの破壊的操作を拒否する。
- scan中は対象windowへの破壊的操作を拒否する。
- file artifactは後続のファイル処理commandの入力になる。

## scan中に拒否する操作

| 操作 | 理由 |
| --- | --- |
| 対象camera roleのopen/close | scan workerがcaptureに使う。 |
| 対象projector roleの変更 | scan workerがpattern表示に使う。 |
| 対象window roleのopen/close | projector表示先である。 |
| manual capture | scan workerのcaptureと競合する。 |
| calibration capture | scan workerのcaptureと競合する。 |

拒否時のerror codeは `scan_resource_busy` である。
