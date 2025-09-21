// src/main.cpp
#include "logger/logger_setup.hpp"
#include <spdlog/spdlog.h>

int main() {
  public_logger::init();                // ← 一度だけ
  SPDLOG_INFO("server start port={}", 8080);   // マクロ（最短）
  spdlog::warn("slow request={}ms", 512);      // 関数でもOK
}
