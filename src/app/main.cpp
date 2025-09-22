// app/main.cpp
#include "app/app.hpp"
#include "logger/logger_setup.hpp"
#include "logger/logger_macros.hpp"

int main() {
  public_logger::init("logs/app.log");
  LOG_INFO("起動");

  App app;
  app.run();

  LOG_INFO("終了");
  return 0;
}
