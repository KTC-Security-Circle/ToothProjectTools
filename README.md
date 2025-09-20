# structured\_light\_decoder

最小・高速志向の C++20 プロジェクト雛形です。
**CMake + Ninja + mold** を採用し、\*\*OpenCV（APT）\*\*と **spdlog（FetchContent）** を使用します。VS Code devcontainer で開発できます（拡張・設定同梱）。&#x20;

---

## スタック

* **ビルド**: CMake（Ninja）＋ mold リンカ（3.29+なら `CMAKE_LINKER_TYPE MOLD`、それ未満は `-fuse-ld=mold` を付与）
* **依存**: OpenCV（APT の `find_package`）、spdlog（FetchContent v1.15.3）
* **静的解析**: cppcheck（ターゲット単位で有効化）
* **開発環境**: VS Code devcontainer（CMake/Ninja 生成、cppcheck 拡張を推奨）

---

## リポジトリ構成（抜粋）

```
.
├─ src/
│   └─ main.cpp        # とりあえず Hello 出力
├─ CMakeLists.txt      # 本プロジェクトのビルド定義
├─ .devcontainer/
│   └─ devcontainer.json
└─ docker-compose.yml  # GUI/X11やカメラデバイスを渡す
```

---

## クイックスタート

### 1) Devcontainer で開く

VS Code で「Reopen in Container」。

* 既定のジェネレータは **Ninja**、C/C++ 拡張や cppcheck 拡張が有効化されます。

> 注: devcontainer の `service` 名は **docker-compose.yml** のサービス名と一致させてください（現状 `structured-service` を参照）。不一致だとアタッチに失敗します。

### 2) ビルド

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

* CMake は Clang/ccache 検出時に **COMPILER\_LAUNCHER** を設定します。
* mold は **CMake 3.29+** なら `CMAKE_LINKER_TYPE MOLD`、それ未満は `-fuse-ld=mold` をリンク時に付与します。

### 3) 実行

```bash
./build/structured_light_decoder
```

---

## 依存関係

### OpenCV（APT）

必要なモジュールのみを `find_package(OpenCV COMPONENTS ...)` でリンクします。HighGUI/VideoIO を使います。

* `opencv_contrib` の **structured\_light / phase\_unwrapping** を使う場合：
  システムに `libopencv-contrib-dev` を追加してください（検出されると自動リンク）。

### spdlog（FetchContent）

`v1.15.3` を FetchContent で取得し（展開タイムスタンプ対策済み）、**ライブラリターゲット** `spdlog::spdlog` をリンクします。ヘッダオンリーに切り替える場合は `spdlog::spdlog_header_only` に変更可。

---

## 静的解析（cppcheck）

* 本体ターゲットにのみ cppcheck を適用しています（`warning, style, performance` の軽量セット）。
* **外部依存（FetchContent の `_deps`）は除外/抑止**して、サードパーティ由来のノイズを避けています。

---

## ログ（spdlog）ヒント

* まずは `#include <spdlog/spdlog.h>` で `spdlog::info("Hello");`。
* 実行時レベル切替には環境変数（例）:

  ```bash
  export SPDLOG_LEVEL="info,app=trace"
  ```

  ※必要ならコード側で `#include <spdlog/cfg/env.h>` → `spdlog::cfg::load_env_levels();` を初期化時に呼ぶ。

---

## GUI / カメラ（X11・V4L2）

* X11 の表示（`imshow` 等）にはホストの X ソケット共有（`/tmp/.X11-unix`）と `DISPLAY` が必要です。
* `/dev/video*` のデバイスを compose 側で渡してください（個別指定または cgroup ルール）。
* iGPU/mesa を使う場合は `/dev/dri` を渡すと描画が軽くなります。

> 具体的な compose 設定は環境依存のため、各自の `docker-compose.yml` に合わせてください。

---

## トラブルシューティング

* **devcontainer が起動しない**
  `devcontainer.json` の `service` 名が compose のサービス名と一致しているか確認。
* **リンクが遅い / mold が効いていない**
  CMake 3.29 未満では `-fuse-ld=mold` を**リンク時に**付与しています（コンパイル時に付くと警告が出ます）。
* **cppcheck の外部警告が出る**
  本ターゲットに対してのみ実行し、`_deps` は除外/抑止設定済みです（CMakeLists を参照）。

---

## 参考（このプロジェクトの定義箇所）

* devcontainer 設定（拡張・Ninja 指定・SSH エージェント共有 など） → `.devcontainer/devcontainer.json` を参照。
* ビルド定義（mold 分岐、OpenCV/Contrib 検出、spdlog FetchContent、cppcheck 設定） → `CMakeLists.txt` を参照。

---

## 今後の拡張のタスク例

* spdlog を **ヘッダオンリー**に切替（ビルド更に高速化） → `target_link_libraries(... spdlog::spdlog_header_only ...)`。
* テスト導入（GoogleTest を FetchContent or APT）
* `PCH`/`Unity Build` のスイッチ追加（大規模化時のビルド時間短縮）
