#pragma once

#include "structured_light/pattern_sync.hpp"

#include <cstdint>
#include <deque>
#include <stdexcept>
#include <string>
#include <utility>

namespace scan
{

structured_light::sync::SyncEvent parsePhotodiodeLine(
    const std::string& line, std::chrono::steady_clock::time_point timestamp, std::uint64_t sequence);

class PhotodiodeTransportError : public std::runtime_error
{
  public:
    PhotodiodeTransportError(std::string code, std::string message)
        : std::runtime_error(std::move(message)), code_(std::move(code)) {}
    const std::string& code() const noexcept { return code_; }

  private:
    std::string code_;
};

class SerialPhotodiodeTransport final : public structured_light::sync::PhotodiodeTransport
{
  public:
    SerialPhotodiodeTransport(const std::string& device, int baud);
    ~SerialPhotodiodeTransport() override;

    SerialPhotodiodeTransport(const SerialPhotodiodeTransport&) = delete;
    SerialPhotodiodeTransport& operator=(const SerialPhotodiodeTransport&) = delete;

    std::optional<structured_light::sync::SyncEvent> receive(std::chrono::milliseconds timeout) override;

  private:
    int fd_{-1};
    std::string buffer_;
    std::deque<structured_light::sync::SyncEvent> pending_events_;
    std::uint64_t sequence_{0};
};

} // namespace scan
