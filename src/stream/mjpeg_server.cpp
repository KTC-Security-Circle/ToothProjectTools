#include "stream/mjpeg_server.hpp"

#include "logger/logger_macros.hpp"
#include "stream/role_name.hpp"
#include "stream/stream_registry.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

namespace stream {
namespace {

bool sendAll(int socket_fd, const void* data, std::size_t size) {
  const auto* cursor = static_cast<const unsigned char*>(data);
  std::size_t remaining = size;

  while (remaining > 0) {
    const auto sent = ::send(socket_fd, cursor, remaining, MSG_NOSIGNAL);
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    if (sent <= 0) {
      return false;
    }

    cursor += sent;
    remaining -= static_cast<std::size_t>(sent);
  }

  return true;
}

bool sendAll(int socket_fd, std::string_view text) {
  return sendAll(socket_fd, text.data(), text.size());
}

std::string roleFromPath(const std::string& path) {
  constexpr std::string_view suffix = ".mjpg";
  if (path.size() <= suffix.size() + 1 || path.front() != '/' ||
      path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
    return {};
  }

  auto role = path.substr(1, path.size() - suffix.size() - 1);
  if (!isValidRoleName(role)) {
    return {};
  }

  return role;
}

} // namespace

MjpegServer::MjpegServer(std::string host, int port, StreamRegistry& registry)
    : host_(std::move(host)), port_(port), registry_(registry) {}

MjpegServer::~MjpegServer() {
  stop();
}

bool MjpegServer::start() {
  // exchange(true) で多重startをatomicに抑止する。
  // 変更前がtrueなら、既に起動済みなのでtrueを返す。
  if (running_.exchange(true)) {
    return true;
  }

  if (host_ != "127.0.0.1") {
    last_error_ = "MJPEG host must be 127.0.0.1";
    running_.store(false);
    return false;
  }

  if (port_ <= 0 || port_ > 65535) {
    last_error_ = "MJPEG port must be between 1 and 65535";
    running_.store(false);
    return false;
  }

  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    last_error_ = std::string("socket failed: ") + std::strerror(errno);
    running_.store(false);
    return false;
  }

  int reuse_address = 1;
  ::setsockopt(
      listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<std::uint16_t>(port_));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    last_error_ = std::string("bind failed: ") + std::strerror(errno);
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_.store(false);
    return false;
  }

  if (::listen(listen_fd_, 8) < 0) {
    last_error_ = std::string("listen failed: ") + std::strerror(errno);
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_.store(false);
    return false;
  }

  try {
    accept_thread_ = std::thread(&MjpegServer::acceptLoop, this);
  } catch (...) {
    last_error_ = "failed to start MJPEG accept thread";
    ::close(listen_fd_);
    listen_fd_ = -1;
    running_.store(false);
    return false;
  }

  LOG_INFO("MJPEG server listening on {}:{}", host_, port_);
  return true;
}

void MjpegServer::stop() {
  // falseへ変更し、変更前がfalseなら停止済みとして何もしない。
  if (!running_.exchange(false)) {
    return;
  }

  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
  }

  // waitForFrame中のclient threadを起こして終了へ向かわせる。
  registry_.wakeAll();

  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }
  listen_fd_ = -1;

  {
    std::lock_guard lock(clients_mutex_);
    for (int client_fd : client_fds_) {
      ::shutdown(client_fd, SHUT_RDWR);
    }
  }

  for (auto& client_thread : client_threads_) {
    if (client_thread.joinable()) {
      client_thread.join();
    }
  }

  client_threads_.clear();
  client_fds_.clear();
}

bool MjpegServer::running() const noexcept {
  return running_.load();
}

const std::string& MjpegServer::lastError() const noexcept {
  return last_error_;
}

void MjpegServer::acceptLoop() {
  while (running_.load()) {
    const int client_fd = ::accept(listen_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (running_.load() && errno != EINTR) {
        LOG_ERROR("MJPEG accept failed: {}", std::strerror(errno));
      }
      continue;
    }

    std::lock_guard lock(clients_mutex_);
    client_fds_.push_back(client_fd);
    client_threads_.emplace_back(&MjpegServer::handleClient, this, client_fd);
  }
}

void MjpegServer::handleClient(int client_fd) {
  timeval send_timeout{2, 0};
  ::setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

  std::string request;
  std::array<char, 2048> buffer{};
  while (request.size() < 8192 && request.find("\r\n\r\n") == std::string::npos) {
    const auto received = ::recv(client_fd, buffer.data(), buffer.size(), 0);
    if (received <= 0) {
      break;
    }
    request.append(buffer.data(), static_cast<std::size_t>(received));
  }

  std::istringstream request_line_stream(request);
  std::string method;
  std::string path;
  std::string protocol;
  request_line_stream >> method >> path >> protocol;

  const auto query_position = path.find('?');
  if (query_position != std::string::npos) {
    path.erase(query_position);
  }

  const auto role = roleFromPath(path);

  if (method != "GET" || protocol.rfind("HTTP/", 0) != 0) {
    sendStatus(client_fd, 400, "Bad Request");
  } else if (role.empty() || !registry_.isStreaming(role)) {
    sendStatus(client_fd, 404, "Not Found");
  } else {
    const std::string headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Connection: close\r\n\r\n";

    if (sendAll(client_fd, headers)) {
      std::uint64_t sequence = 0;
      while (running_.load() && registry_.isStreaming(role)) {
        auto frame =
            registry_.waitForFrame(role, sequence, std::chrono::milliseconds(500));
        if (!frame) {
          continue;
        }

        sequence = frame->sequence;

        std::ostringstream part_headers;
        part_headers << "--frame\r\n"
                     << "Content-Type: image/jpeg\r\n"
                     << "Content-Length: " << frame->bytes.size() << "\r\n\r\n";

        if (!sendAll(client_fd, part_headers.str()) ||
            !sendAll(client_fd, frame->bytes.data(), frame->bytes.size()) ||
            !sendAll(client_fd, "\r\n")) {
          break;
        }
      }
    }
  }

  {
    std::lock_guard lock(clients_mutex_);
    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
    if (it != client_fds_.end()) {
      client_fds_.erase(it);
    }
  }

  ::shutdown(client_fd, SHUT_RDWR);
  ::close(client_fd);
}

void MjpegServer::sendStatus(
    int client_fd,
    int status,
    const std::string& reason) {
  std::ostringstream response;
  response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
           << "Content-Type: text/plain; charset=utf-8\r\n"
           << "Content-Length: " << reason.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << reason;

  sendAll(client_fd, response.str());
}

} // namespace stream
