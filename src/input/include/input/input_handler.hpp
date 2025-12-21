#pragma once
#include <functional>
#include <unordered_map>

namespace input {

/**
 * @brief キーコード→アクション（関数）を管理するシンプルな入力ハンドラ。
 * 
 * - bind(key, action) で登録、handle(key) で実行。
 * - Action 型は「引数なし／戻り値なし」の callable（関数・ラムダ等）。
 * - スレッドセーフではない（通常のGUIループ内での単一スレッド使用を想定）。
 */
class InputHandler {
public:
  using Action = std::function<void()>;

  void bind(int keycode, Action action);   ///< 上書き登録（同一キーは差し替え）
  void unbind(int keycode);                ///< キー割り当てを削除
  bool bound(int keycode) const;           ///< 登録済みか？
  void clear();                            ///< 全解除

  /// @brief pollKey/waitKey 等の戻り値を渡すと、対応アクションを実行
  void handle(int keycode) const;

private:
  std::unordered_map<int, Action> map_;
};

}
