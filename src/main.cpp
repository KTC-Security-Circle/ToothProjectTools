// src/main.cpp
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h> // SPDLOG_LEVEL=... を読むなら

#include <iostream>

int main() {
    spdlog::info("Hello C++");
    return 0;
}
