# 状態

## serve app

```text
starting -> ready -> running -> shutting_down -> stopped
                         -> failed
```

| state | 意味 |
| --- | --- |
| `starting` | process起動中である。 |
| `ready` | MJPEG serverが起動し、ready eventを出した。 |
| `running` | JSON Lines commandを受け付ける。 |
| `shutting_down` | shutdown処理中である。 |
| `stopped` | processが終了した。 |
| `failed` | 起動または処理に失敗した。 |

現在の実装はserve app stateをfieldとして公開しない。
外部から観測できる状態はready event、return、process exit codeである。

## scan

```text
idle -> running -> completed
              -> stopping -> failed
              -> stopping -> stopped
```

| state | 意味 |
| --- | --- |
| `idle` | active scanがない。 |
| `running` | scan workerが動作中である。 |
| `stopping` | stop要求を受けた。 |
| `completed` | 全patternのcaptureが完了した。 |
| `failed` | scanが失敗した。 |

## scan_startの非同期仕様

| 種別 | 意味 |
| --- | --- |
| return | command受付結果。 |
| event | 実際のscan進捗、完了、失敗。 |

## scan event

| event | 意味 |
| --- | --- |
| `scan_started` | workerが開始した。 |
| `scan_frame_captured` | 左右画像を1組保存した。 |
| `scan_completed` | 全画像を保存した。 |
| `scan_stopping` | stop要求を受けた。 |
| `scan_stopped` | workerが停止した。 |
| `scan_failed` | workerが失敗した。 |
