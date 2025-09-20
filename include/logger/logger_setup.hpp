// logger_setup.hpp
#pragma once
#include <string>

namespace public_logger {

/// @brief spdlog の既定ロガーを初期化（コンソール＋ローテーションファイル）
/// @param logfile ログファイルパス
/// @param env_console_level コンソールレベルを読む環境変数名（例: "APP_CONSOLE_LEVEL"）
/// @param rotate_bytes 1ファイルの最大サイズ（バイト）
/// @param rotate_files 保持世代数
void init(const std::string& logfile = "logs/app.log",
        const char* env_console_level = "APP_CONSOLE_LEVEL",
        std::size_t rotate_bytes = 5 * 1024 * 1024,
        std::size_t rotate_files = 3);

} // namespace public_logger
