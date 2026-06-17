#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace stream {

class StreamRegistry;

// localhostだけにbindする最小MJPEG server。
// StreamRegistryから最新JPEGを取り出し、multipart/x-mixed-replaceで配信する。
class MjpegServer {
public:
  MjpegServer(std::string host, int port, StreamRegistry& registry);
  ~MjpegServer();

  // socket fd / worker thread / client threadを持つため、copy/moveともに禁止する。
  MjpegServer(const MjpegServer&) = delete;
  MjpegServer& operator=(const MjpegServer&) = delete;
  MjpegServer(MjpegServer&&) = delete;
  MjpegServer& operator=(MjpegServer&&) = delete;

  bool start();
  void stop();
  bool running() const noexcept;
  const std::string& lastError() const noexcept;

private:
  void acceptLoop();
  void handleClient(int client_fd);
  void sendStatus(int client_fd, int status, const std::string& reason);

  std::string host_;
  int port_;
  StreamRegistry& registry_;

  // start/stop/acceptLoop/handleClientから触られるためatomicで管理する。
  std::atomic<bool> running_{false};

  int listen_fd_{-1};
  std::thread accept_thread_;

  std::mutex clients_mutex_;
  std::vector<int> client_fds_;

  // MVPではthread per client。
  // 長時間稼働時は終了済みthreadのreapを後続課題にする。
  std::vector<std::thread> client_threads_;

  std::string last_error_;
};

} // namespace stream
