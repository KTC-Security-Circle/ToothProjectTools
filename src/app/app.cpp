#include "app/app.hpp"
#include "app/systems.hpp"

// コンストラクタ
App::App() : ctx_(std::make_unique<app::AppContext>()) {}

// デストラクタ
App::~App() = default;

// 実行エントリポイント
void App::run() {
  // 初期化 (sys_init.cpp で実装)
  app::sys::setup_app(*ctx_);
  
  // メインループ開始
  loop();
}

// メインループ
void App::loop() {
  while (ctx_->running) {
    // 1. 入力処理 (sys_input.cpp)
    app::sys::process_input(*ctx_);

    // 2. コマンド処理 (dispatch/...)
    app::sys::process_commands(*ctx_);
    
    // 3. 更新処理 (sys_update.cpp)
    app::sys::update_scan(*ctx_);
    app::sys::update_preview(*ctx_);
    
    // 4. 描画処理 (sys_render.cpp)
    app::sys::render_all(*ctx_);
  }
}