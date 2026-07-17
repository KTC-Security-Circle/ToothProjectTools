# structured_light_decoder

構造光キャリブレーションのための最小・高速志向 C++20 アプリケーションです。  
**CMake + Ninja + mold + OpenCV + spdlog** を採用し、ゲームループ構造とコマンドキューを基盤にした柔軟なマルチウィンドウ制御を実現します。

本プロジェクトは **カメラ・プロジェクターを用いた外部パラメータ推定** を通じて、  
ステレオビジョンおよび構造光による **3D復元** を最終目標としています。

---

## 🛠 技術スタック

- **言語 / 標準**: C++20 （型安全・`std::variant`によるイベント表現）
- **ビルド**: CMake（Ninja）＋ mold リンカ（高速リンク）:contentReference[oaicite:0]{index=0}
- **依存ライブラリ**:
  - OpenCV 4.x（HighGUI, VideoIO, structured_light, phase_unwrapping）
  - spdlog（環境変数でのログレベル制御, rotateファイル出力）:contentReference[oaicite:1]{index=1}
- **解析ツール**: cppcheck（外部依存を除外し、本体ターゲットのみを解析）
- **開発環境**:
  - Arch Linux
  - VS Code devcontainer（Docker Compose 経由で GUI/X11, V4L2, iGPU をホスト共有）

---

## 🚀 クイックスタート

### 1. ビルド
```bash
make build
````

（内部で以下を実行しています）

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 2. 実行

```bash
./build/structured_light_decoder
```

### 3. クリーン

```bash
make clean
```

---

## 📂 リポジトリ構成

```text
.
├── CMakeLists.txt
├── Dockerfile
├── Makefile
├── README.md
├── cppcheck.supp
├── docker-compose.yml
└── docs
    ├── architecture
    │   ├── overview.md             # 全体構成
    │   ├── concepts.md             # 主要概念
    │   └── command-flow.md         # command処理の流れ
    ├── artifacts                   # scan dataset / decode resultなどのartifact仕様
    ├── control                     # JSONL protocolとcommand仕様
    ├── testing                     # test方針と手動hardware test
    ├── appパッケージ設計メモ.md       # app/ の設計意図とゲームループ/ディスパッチ構造
    ├── cmdパッケージ設計メモ.md       # cmd/ のエンベロープ方式（Target + Command）
    ├── inputパッケージ設計メモ.md     # 入力→コマンド変換と既定バインディング
    ├── logger パッケージ設計メモ.md   # spdlog ベースのロギング（source_loc マクロ等）
    ├── videoパッケージ設計メモ.md     # カメラ抽象化/二重バッファ/近似同期(Δt)
    └── window パッケージ設計メモ.md   # ウィンドウ抽象/レイアウト/FS化/モニタ移動

src/
├── app/      # アプリ本体（ループ・dispatch・マウス処理）
├── cmd/      # Command / Target / DispatchCmd 定義（INTERFACE想定）
├── input/    # InputHandler, デフォルトバインディング
├── logger/   # spdlog セットアップ（マクロ/初期化）
├── video/    # Camera 入力
└── window/   # Window 抽象化, Monitor情報, X11対応
```

---

## 📖 設計ドキュメント

* [`docs/architecture/overview.md`](./docs/architecture/overview.md) — 全体構成
* [`docs/architecture/concepts.md`](./docs/architecture/concepts.md) — 主要概念
* [`docs/architecture/command-flow.md`](./docs/architecture/command-flow.md) — command処理の流れ
* [`docs/control/protocol.md`](./docs/control/protocol.md) — JSONL control protocol
* [`docs/control/command-index.md`](./docs/control/command-index.md) — command一覧
* [`docs/artifacts/scan-dataset.md`](./docs/artifacts/scan-dataset.md) — scan dataset仕様
* [`docs/testing/test-strategy.md`](./docs/testing/test-strategy.md) — test方針

---

## 📛 バッジ例

![C++](https://img.shields.io/badge/C++-20-blue.svg?style=for-the-badge)
![CMake](https://img.shields.io/badge/CMake-3.29+-green.svg?style=for-the-badge)
![OpenCV](https://img.shields.io/badge/OpenCV-4.x-orange.svg?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-lightgrey.svg?style=for-the-badge)

---

## 💡 開発者向けTips

* **Devcontainer**: VS Code から「Reopen in Container」で即環境構築
* **X11 / V4L2**: GUI出力やカメラ入力は docker-compose でホストの `/tmp/.X11-unix`, `/dev/video*` を渡す必要あり
* **ログ**: `export SPDLOG_LEVEL="info,app=trace"` で実行時レベル切替
* **静的解析**: `cppcheck` は外部依存を除外済み（ノイズを抑制）

---

## 🧩 ライセンス
[MIT LICENSE](./LICENSE)
