# 概要

ToothProjectTools の `tooth-backend serve` は、C++ sidecarとして動作する。
serve app は標準入力からJSON Linesを受け取る。
responseとeventはstdoutへJSON Linesで出力する。
logger、OpenCV診断、process障害情報はstderrへ出力する。

MJPEGはpreview配信用である。
MJPEGはControl Messageとは別である。
stdoutへJPEG bytesやMJPEG multipart bodyは出力しない。

## 役割

| 対象 | 役割 |
| --- | --- |
| Tauri Web UI | commandを作る。previewを表示する。 |
| Tauri Rust backend | sidecar processを起動する。stdin/stdout/stderrを中継する。 |
| C++ sidecar | camera、stream、capture、scan、calibration、decodeを扱う。 |
| CameraManager | OpenCV camera deviceをopen/closeする。 |
| MJPEG server | camera frameをlocalhost HTTPで配信する。 |

## 所有ルール

- Tauri / Rust はcamera deviceを直接openしない。
- C++ sidecarがcamera deviceをopenする。
- C++ sidecarがstream、capture、scanを実行する。
- MJPEGは既存Window表示を置き換えない。
- JSON Linesのreturnとeventはstdoutへ出す。
- logと診断情報はstderrへ出す。

## 関連docs

| 内容 | docs |
| --- | --- |
| 概念定義 | [concepts.md](./concepts.md) |
| コマンド処理 | [command-flow.md](./command-flow.md) |
| リソース所有 | [resource-ownership.md](./resource-ownership.md) |
| 状態 | [state.md](./state.md) |
| JSON Lines仕様 | [../control/protocol.md](../control/protocol.md) |
| コマンド一覧 | [../control/command-index.md](../control/command-index.md) |
| Artifact仕様 | [../artifacts/scan-dataset.md](../artifacts/scan-dataset.md) |
| テスト方針 | [../testing/test-strategy.md](../testing/test-strategy.md) |
