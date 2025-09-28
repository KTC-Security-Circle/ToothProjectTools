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
├── Appパッケージ設計メモ.md       # app/ の設計意図とゲームループ構造
├── cmdパッケージ設計メモ.md       # cmd/ のエンベロープ方式
├── inputパッケージ設計メモ.md     # input/ の入力処理と既定バインディング
├── README.md
├── Makefile                      # make build / make clean
├── docker-compose.yml / Dockerfile
├── src
│   ├── app/      # アプリ本体（ループ・dispatch・マウス処理）
│   ├── cmd/      # Command / Target / DispatchCmd 定義
│   ├── input/    # InputHandler, デフォルトバインディング
│   ├── logger/   # spdlog セットアップ
│   ├── video/    # Camera 入力
│   └── window/   # Window 抽象化, Monitor情報, X11対応
└── docs
    └── architecture.md           # パッケージ横断の統合ドキュメント
```

---

## 📖 設計ドキュメント

* [docs/architecture.md](./docs/architecture.md) — 全体の設計概要とPlantUML図
* [Appパッケージ設計メモ](./Appパッケージ設計メモ.md) — ゲームループ構造とコマンドディスパッチ
* [cmdパッケージ設計メモ](./cmdパッケージ設計メモ.md) — エンベロープ方式とCommand/Targetの分離
* [inputパッケージ設計メモ](./inputパッケージ設計メモ.md) — 入力→コマンド変換, 既定バインディング

---

## 🎯 今後の想定

1. **ステレオキャリブレーション**

   * チェッカーボードを用いてカメラ2台の外部パラメータを推定
   * OpenCV `stereoCalibrate` ベースの実装

2. **構造光キャリブレーション**

   * OpenCV `structured_light::GrayCodePattern` を用い、モニタ全画面に投影
   * カメラウィンドウと同期して撮影・保存

3. **3D復元**

   * 得られたパラメータを基に対象物の点群を生成
   * 将来的にはメッシュ化／可視化ツールとの連携も想定

4. **機能拡張予定**

   * コマンド記録・リプレイ（自動テスト, デバッグ）
   * JSON/IPC 経由のコマンド投入（スクリプト制御）
   * TargetGroupによるウィンドウ群制御（複数画面対応）
   * GoogleTest によるユニットテスト導入
   * PCH / Unity Build オプション（ビルド時間短縮）

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
後で追加します
