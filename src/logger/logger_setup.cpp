#include "logger/logger_setup.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include <spdlog/cfg/env.h> // load_env_levels()
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace {

// 出力先ごとのフォーマット文字列
// %^ .. %$ で色付け範囲（コンソールのみ）
// %l はレベル名（info/warn/error/...）
constexpr const char* console_pattern = "[%s:%# %!][%^%l%$] %v";
constexpr const char* file_pattern    = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%# %!] %v";

// "error" を "err" に寄せるなどのゆるい正規化
inline std::string normalize_level_string(std::string level_string) {
    std::transform(level_string.begin(), level_string.end(), level_string.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (level_string == "error") {
        level_string = "err";
    }
    return level_string;
}

inline spdlog::level::level_enum parse_level_or_fallback(const char* environment_value,
                                                         spdlog::level::level_enum fallback_level) {
    if (!environment_value || !*environment_value) {
        return fallback_level;
    }
    auto normalized_string = normalize_level_string(environment_value);
    auto parsed_level = spdlog::level::from_str(normalized_string); // trace/debug/info/warn/err/critical/off
    if (normalized_string == "off") {
        return spdlog::level::off; // 明示 off は尊重
    }
    return (parsed_level == spdlog::level::off && normalized_string != "off")
               ? fallback_level
               : parsed_level;
}

} // namespace

namespace public_logger {

void init(const std::string& logfile,
          const char* environment_variable_for_console_level,
          std::size_t rotate_bytes,
          std::size_t rotate_files) {
    // 環境変数 SPDLOG_LEVEL で既定ロガー/規定レベルを設定可能（"info,app=trace" 等）
    spdlog::cfg::load_env_levels();

    // コンソール sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    const char* environment_value_for_console_level =
        std::getenv(environment_variable_for_console_level); // NOLINT(concurrency-mt-unsafe)
    auto console_level =
        parse_level_or_fallback(environment_value_for_console_level, spdlog::level::info);
    console_sink->set_level(console_level);
    console_sink->set_pattern(console_pattern);

    // ファイル sink（ローテーション）。ディレクトリは必要なら作成
    try {
        std::filesystem::path logfile_path{logfile};
        if (logfile_path.has_parent_path()) {
            std::filesystem::create_directories(logfile_path.parent_path());
        }
    } catch (...) {
        // 失敗してもコンソール出力は生きる
    }

    auto file_sink =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logfile, rotate_bytes, rotate_files);
    file_sink->set_level(spdlog::level::trace); // ファイルは詳細に残す
    file_sink->set_pattern(file_pattern);

    // 統合ロガーを既定に据える
    auto combined_logger =
        std::make_shared<spdlog::logger>("app", spdlog::sinks_init_list{console_sink, file_sink});
    combined_logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(combined_logger);

    SPDLOG_INFO("logger initialized: console_level={}, file_rotate={} bytes, keep {} files",
                spdlog::level::to_string_view(console_level), rotate_bytes, rotate_files);
}

} // namespace public_logger
