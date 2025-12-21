#pragma once
#include <memory>
#include "app/app_context.hpp"

// 前方宣言
namespace app::sys { void setup_app(AppContext&); }

class App {
public:
  App();
  ~App();

  // アプリケーション実行（初期化 -> ループ）
  void run();

private:
  // メインループ
  void loop();

private:
  // アプリケーションの状態データはすべてここに集約
  std::unique_ptr<app::AppContext> ctx_;
};