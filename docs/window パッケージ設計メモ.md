# ウィンドウパッケージ設計メモ（window）

> 対象: `src/window`、`WindowService`、`MonitorService`

`window` packageは、ProjectorへGray Code patternを表示するためのdisplay backendである。
keyboard入力、camera preview、ユーザー操作用GUI application loopは責務に含めない。

## 実行経路

```text
JSONL projector command
    ↓
HeadlessDispatcher / ProjectorService
    ↓
WindowService
    ↓
WindowManager / Window
    ↓
OpenCV HighGUI + X11/XRandR
```

`ProjectorService::showPattern()`はpattern canvasを構築し、`WindowService::showImage()`へ渡す。
`ServeApp`のmain threadは`WindowService::processPendingRequests()`を実行し、windowが開いている間は
`WindowService::pollEvents()`でHighGUI eventを進行させる。

## 責務

- Projector用windowの生成と破棄
- `cv::Mat`画像の提示
- monitor位置・寸法の取得
- window位置・寸法およびfullscreen状態の設定
- HighGUI event pump
- worker threadから受けたwindow requestのmain thread実行

Camera frameの取得、keyboard操作、scan進行、pattern生成はこのpackageの責務ではない。

## Threadとlifetime

HighGUI操作は`ServeApp`を実行するmain threadへ限定する。
JSONL control threadやscan workerからの要求は`WindowService`のrequest queueを経由する。
`SidecarService`が`WindowManager`、`MonitorService`、`WindowService`、`ProjectorService`を所有し、
参照されるmanagerとserviceは参照元より長く生存する。

## Headless環境

`tooth-backend serve --control stdio`の起動自体はDISPLAYを要求しない。
Projector/window commandを使用しない限りHighGUI backendを初期化できない環境でもserve処理を継続する。
物理Projectorへ表示するcommandには、利用可能なdisplay backendとmonitorが必要である。

## 依存関係

- `window` targetはOpenCV HighGUIを含む`cv_lib`へ依存する。
- Linux monitor列挙とwindow配置のためX11/XRandRへ依存する。
- `window_service`は`window`と`monitor_service`へ依存する。
- `projector_service`は`window_service`と`monitor_service`へ依存する。
- `scan_service`は`projector_service`を介してpatternを提示する。

HighGUI完全削除やSDL、Wayland、DRM/KMS等への移行は別のbackend変更として扱う。
