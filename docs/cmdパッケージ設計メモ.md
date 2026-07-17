# コマンドパッケージ設計メモ（cmd）（刷新版）

> 対象: `./src/cmd/include/cmd/{commands.hpp, target.hpp, dispatch_cmd.hpp, keycodes.hpp}`
>
> **位置づけ**: `cmd` は **副作用を持たないデータモデル**（Command/Target/Envelope）。概念的には `input` の“子”として運用しますが、ビルド上は **INTERFACE ライブラリ**として独立させ、`input` が **PUBLIC 依存**で再輸送（re-export）します。上位の `app` は `input` にリンクすれば `cmd` も利用可能になります。

---

## 1. 目的と責務（Scope）

* **操作要求の正規化**：アプリの操作を **“何を（Command）× どこへ（Target）”** のデータとして表現。
* **副作用ゼロ**：`cmd` 自身は実行しない（=テスト容易・シリアライズ容易）。
* **単一経路の副作用**：実行は常に `app::dispatch` 側で行われる前提（`window` API への橋渡し）。

非責務：入力収集（`input`）、描画（`window`）、アプリ状態更新（`app`）。

---

## 2. 公開API（案）

```cpp
// commands.hpp（抜粋）
namespace cmd {
  struct CmdQuit               {};
  struct CmdToggleFullscreen   {};
  struct CmdMoveToMonitor      { int index; };
  struct CmdFocusNext          {};

  using Command = std::variant<
    CmdQuit,
    CmdToggleFullscreen,
    CmdMoveToMonitor,
    CmdFocusNext
  >;
}
```

```cpp
// target.hpp（抜粋）
namespace win { using WindowId = std::uint32_t; } // 循環回避のための前方宣言/別名

namespace cmd {
  struct TargetAll       {};
  struct TargetFocused   {};
  struct TargetById      { win::WindowId id; };

  using Target = std::variant<TargetAll, TargetFocused, TargetById>;
}
```

```cpp
// dispatch_cmd.hpp（抜粋）
namespace cmd {
  struct DispatchCmd {
    Target  target;
    Command cmd;
  };
}
```

```cpp
// keycodes.hpp（抜粋）
namespace cmd {
  // HighGUI の戻り値差を吸収する論理キー定義（例）
  enum class Key {
    Esc, Q, F, Tab, Num1, Num2,
    // ... add here
  };
}
```

> **設計要点**：`std::variant` により **列挙漏れをコンパイルエラー化**。新規コマンド追加時は `std::visit` の到達性が欠けるとビルドに失敗するため、運用での漏れに強い。

---

## 3. ライフサイクル / データフロー

1. **生成**：`input` がキー入力を受け、`DispatchCmd{Target, Command}` を生成してキューに push。
2. **適用**：`app` がキューを取り出して宛先解決し、`window` API に橋渡し。
3. **ログ**：発行側（input）では「生成ログ」、適用側（app）では「適用ログ」を出し、観測点を分離。

---

## 4. パッケージ境界 & 再輸送（re-export）

* **ビルド単位**：`add_library(cmd_api INTERFACE)`
* **包括ヘッダ**：`include/cmd/cmd.hpp` を用意して commands/target/dispatch を一括インクルード（利用者利便性）。
* **依存の流れ**：

  * `input` は `target_link_libraries(input PUBLIC cmd_api)`
  * `app` は `target_link_libraries(app_target PRIVATE input)`
  * `window` は `cmd` を参照しない（副作用点ではないため）

---

## 5. シリアライズ / ログ / リプレイ

* **安定フィールド**：コマンドのフィールドは POD/プリミティブ中心に保つ（JSON/CBOR 化しやすい）。
* **バージョニング**：フィールド追加は後方互換（optional）を意識。破壊的変更は `version` タグで切替。
* **リプレイ**：`DispatchCmd` 列を保存→再生してバグ再現/E2Eテストに使用。

---

## 6. 循環依存の回避

* `TargetById` は `win::WindowId` に依存 → **前方宣言/別名**で重いヘッダの依存を避ける。
* `cmd` は **実装を持たない**ため、`cpp` ファイル不要（ヘッダ専用）。

---

## 7. スレッド & パフォーマンス

* `cmd` 自体は **スレッド安全の責務なし**（ただのデータ）。
* 複数スレッドでの生産/消費は **キュー側（`app`）で同期**。
* `std::variant` はコピーコストが小さい構造体に留める（大きなバッファは持たない）。

---

## 8. CMake / 依存関係

```cmake
# ビルド定義（src/cmd/CMakeLists.txt）
add_library(cmd_api INTERFACE)

target_include_directories(cmd_api INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

* `input` が **PUBLIC** に `cmd_api` を依存、`app` は `input` に依存。
* サブディレクトリの読み込み順：`src/cmd` → `src/input` → `src/window` → `src/app`。

---

## 9. テスト & 検証チェックリスト

* [ ] 代表コマンド追加時に `std::visit` が**網羅チェック**を担保していること（ビルド失敗テスト）。
* [ ] `TargetById` の `WindowId` が **未割当(0)不許可**などの前提を満たすか（静的アサート）。
* [ ] シリアライズ/デシリアライズの往復で**同値性**が成立。
* [ ] JSON 互換の文字列/整数範囲でフィールドが表現可能。

---

## 10. 将来拡張

* **Window系**：`CmdSetLayout{LayoutMode}`, `CmdSetScale{double}`, `CmdShowOverlay{bool}`
* **グルーピング**：`TargetGroup{std::vector<WindowId>}`、`TargetClass{enum}` など
* **タイミング**：`CmdDelay{ms}`, `CmdAt{timestamp}`（記録/再生用）
* **外部注入**：CLI/JSON/IPC から `DispatchCmd` を受け付ける **スクリプト入力**

---

## 11. PlantUML（データモデル）

```plantuml
@startuml
package cmd {
  class DispatchCmd {
    +target: Target
    +cmd: Command
  }

  class Command <<variant>> {
  }
  class Target <<variant>> {
  }

  class CmdQuit
  class CmdToggleFullscreen
  class CmdMoveToMonitor { +index: int }
  class CmdFocusNext

  class TargetAll
  class TargetFocused
  class TargetById { +id: WindowId }
}

Command <|-- CmdQuit
Command <|-- CmdToggleFullscreen
Command <|-- CmdMoveToMonitor
Command <|-- CmdFocusNext

Target <|-- TargetAll
Target <|-- TargetFocused
Target <|-- TargetById

DispatchCmd o--> Command
DispatchCmd o--> Target
@enduml
```

---

## 12. 実装ポリシー（要点）

* **副作用禁止**：`cmd` は**実行しない**。あくまでデータ表現に徹する。
* **拡張容易性**：`std::variant` の列挙に追加し、訪問側（`app`）で網羅パターンマッチ。
* **命名整合**：ログ文言・キー名・コマンド名を一致させる（ドキュメントとコードを同期）。
* **コメント規約**：各コマンドの**目的・トリガ・副作用先**を1行で明記（例：`// F: Toggle focused window fullscreen`）。

---

### 付録：新コマンド追加手順（例）

1. `commands.hpp` に `struct CmdSetLayout { LayoutMode m; };` を追加。
2. `using Command = std::variant<..., CmdSetLayout>;` を更新。
3. `input` のバインディングにキーを追加し、`DispatchCmd{TargetFocused, CmdSetLayout{...}}` を push。
4. `app::applyCommandToWindow_` に訪問分岐を追加。
5. ドキュメント（本メモ/README）を更新。
