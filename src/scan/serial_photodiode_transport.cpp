#include "scan/serial_photodiode_transport.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace scan
{
structured_light::sync::SyncEvent parsePhotodiodeLine(
    const std::string& line, std::chrono::steady_clock::time_point timestamp, std::uint64_t sequence)
{
    if (line != "0" && line != "1")
        throw PhotodiodeTransportError("photodiode_invalid_event", "invalid photodiode event: " + line);
    return {line == "0" ? structured_light::sync::MarkerState::black
                        : structured_light::sync::MarkerState::white,
            timestamp, sequence, structured_light::sync::SyncSource::photodiode, 1.0};
}

namespace
{

speed_t baudConstant(int baud)
{
    switch (baud)
    {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
#ifdef B230400
    case 230400: return B230400;
#endif
    default: throw PhotodiodeTransportError("photodiode_open_failed", "unsupported photodiode baud: " + std::to_string(baud));
    }
}

std::string openErrorCode(int error)
{
    if (error == ENOENT || error == ENODEV) return "photodiode_device_not_found";
    if (error == EACCES || error == EPERM) return "photodiode_permission_denied";
    return "photodiode_open_failed";
}

} // namespace

SerialPhotodiodeTransport::SerialPhotodiodeTransport(const std::string& device, int baud)
{
    const auto speed = baudConstant(baud);
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0)
    {
        const auto error = errno;
        throw PhotodiodeTransportError(openErrorCode(error), device + ": " + std::strerror(error));
    }

    termios options{};
    if (::tcgetattr(fd_, &options) != 0)
    {
        const auto message = std::string{"failed to read serial settings: "} + std::strerror(errno);
        ::close(fd_);
        fd_ = -1;
        throw PhotodiodeTransportError("photodiode_open_failed", message);
    }
    ::cfmakeraw(&options);
    ::cfsetispeed(&options, speed);
    ::cfsetospeed(&options, speed);
    options.c_cflag |= CLOCAL | CREAD;
    if (::tcsetattr(fd_, TCSANOW, &options) != 0)
    {
        const auto message = std::string{"failed to configure serial device: "} + std::strerror(errno);
        ::close(fd_);
        fd_ = -1;
        throw PhotodiodeTransportError("photodiode_open_failed", message);
    }
    ::tcflush(fd_, TCIFLUSH);
}

SerialPhotodiodeTransport::~SerialPhotodiodeTransport()
{
    if (fd_ >= 0) ::close(fd_);
}

std::optional<structured_light::sync::SyncEvent>
SerialPhotodiodeTransport::receive(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (!pending_events_.empty())
        {
            auto event = pending_events_.front();
            pending_events_.pop_front();
            return event;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        pollfd descriptor{fd_, POLLIN, 0};
        const int result = ::poll(&descriptor, 1, static_cast<int>(std::max<std::int64_t>(1, remaining.count())));
        if (result == 0) return std::nullopt;
        if (result < 0)
        {
            if (errno == EINTR) continue;
            throw PhotodiodeTransportError("photodiode_open_failed", std::string{"serial poll failed: "} + std::strerror(errno));
        }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
            throw PhotodiodeTransportError("photodiode_open_failed", "photodiode serial device disconnected");
        char bytes[128];
        const auto count = ::read(fd_, bytes, sizeof(bytes));
        if (count > 0)
        {
            const auto received_at = std::chrono::steady_clock::now();
            buffer_.append(bytes, static_cast<std::size_t>(count));
            for (auto newline = buffer_.find('\n'); newline != std::string::npos;
                 newline = buffer_.find('\n'))
            {
                auto line = buffer_.substr(0, newline);
                buffer_.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                pending_events_.push_back(parsePhotodiodeLine(line, received_at, ++sequence_));
            }
        }
        else if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            throw PhotodiodeTransportError("photodiode_open_failed", std::string{"serial read failed: "} + std::strerror(errno));
    }
    return std::nullopt;
}

} // namespace scan
