# `cmd` パッケージ設計メモ

（コマンド／ターゲット／ディスパッチ・エンベロープ）

> 対象: `./src/cmd/include/cmd/{commands.hpp, target.hpp, dispatch_cmd.hpp, keycodes.hpp}`

---

## 目的と責務

* **アプリの操作要求を「データ」として表現**し、どこからでも同一の形式で投げられるようにする。
* \*\*“何を（Command）”“どこへ（Target）”\*\*を一体化した \*\*エンベロープ（`DispatchCmd`）\*\*を提供。
* 送信側は副作用を持たず、**実行は `App::dispatch` 側**で責務分離。

---

## コア型（公開API）

```cpp
// commands.hpp
struct CmdQuit {};
struct CmdToggleFullscreen {};
struct CmdMoveToMonitor { int index; };
struct CmdFocusNext {};
using Command = std::variant<CmdQuit, CmdToggleFullscreen, CmdMoveToMonitor, CmdFocusNext>;

// target.hpp
struct TargetAll {};
struct TargetFocused {};
struct TargetById { WindowId id; };     // ※ WindowId は window パッケージが公開する別名型
using Target = std::variant<TargetAll, TargetFocused, TargetById>;

// dispatch_cmd.hpp
struct DispatchCmd {
  Target  target;
  Command cmd;
};
```

* **拡張**：コマンドを追加する場合は `Command` の `std::variant` に型を追加。
* **独立性**：`DispatchCmd` はデータのみ。実行はどこもしない（テスト・記録・再生が容易）。

---

## データフローと結合点

1. `input` がユーザー入力を受け取り、`DispatchCmd` を生成して **キューへ積む**。
2. `app` がキューから取り出し、**宛先解決 → 実行**を行う（`App::dispatch`）。

* **結合方向**：`app` は `cmd` を **利用**する（`cmd` は `app` を知らない）。
* **ビルド**：`cmd` はヘッダ専用（`INTERFACE` ライブラリ）で公開。

---

## 設計上の注意

* **循環依存を避ける**：`TargetById` が `WindowId` に依存するため、`WindowId` は **前方宣言できる別名型**にしておくと安全。
* **安定したシリアライズ**：将来、ログ保存・ネットワーク伝送を視野に入れるなら、
  コマンドの**フィールドはシリアライズ可能な素直な型**を維持する。

---

## 参考（一般資料）

* *C++ Core Guidelines*（データ指向・責務分離、F.2/F.3 など）
* *cppreference.com*（`std::variant`、パターンマッチ／`std::visit` の基本）
* *Command パターン*（GoF）とイベント・キューイングの設計知見
