#include "logger/logger_setup.hpp"
#include "serve/serve_app.hpp"

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void printUsage(std::ostream& output) {
  output << "Usage: tooth-backend serve --control stdio "
            "[--mjpeg-host 127.0.0.1] [--mjpeg-port 39010]\n";
}

bool parsePort(std::string_view text, int& port) {
  int parsed = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
      parsed < 1 || parsed > 65535) {
    return false;
  }
  port = parsed;
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    printUsage(std::cout);
    return 0;
  }
  if (argc < 2 || std::string_view{argv[1]} != "serve") {
    printUsage(std::cerr);
    return 2;
  }

  serve::ServeOptions options;
  std::string control_mode{"stdio"};

  for (int index = 2; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      printUsage(std::cout);
      return 0;
    }
    if (index + 1 >= argc) {
      std::cerr << "Missing value for " << argument << '\n';
      return 2;
    }

    const std::string value{argv[++index]};
    if (argument == "--control") {
      control_mode = value;
    } else if (argument == "--mjpeg-host") {
      options.mjpeg_host = value;
    } else if (argument == "--mjpeg-port") {
      if (!parsePort(value, options.mjpeg_port)) {
        std::cerr << "Invalid MJPEG port: " << value << '\n';
        return 2;
      }
    } else {
      std::cerr << "Unknown option: " << argument << '\n';
      return 2;
    }
  }

  if (control_mode != "stdio") {
    std::cerr << "Only --control stdio is supported in this MVP\n";
    return 2;
  }
  if (options.mjpeg_host != "127.0.0.1") {
    std::cerr << "--mjpeg-host must be 127.0.0.1\n";
    return 2;
  }

  public_logger::init_sidecar();
  serve::ServeApp app(std::move(options), std::cin, std::cout);
  return app.run();
}
