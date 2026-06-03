#pragma once
#include "runtime/app_context.hpp"

#include <memory>

// 前方宣言
namespace app::sys
{
void setup_app(runtime::AppContext&);
}

class App
{
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
    std::unique_ptr<runtime::AppContext> ctx_;
};