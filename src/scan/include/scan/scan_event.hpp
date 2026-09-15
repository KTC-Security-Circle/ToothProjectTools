#pragma once

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace service::scan
{

struct ScanEvent
{
    /// event <std::string>: JSONL event名。
    std::string event;

    /// values <std::map<std::string, std::string>>: event payload。
    std::map<std::string, std::string> values;
};

class ScanEventSink
{
  public:
    virtual ~ScanEventSink() = default;

    /// @brief scan workerからeventをpushする。
    virtual void push(ScanEvent event) = 0;
};

class ScanEventQueue final : public ScanEventSink
{
  public:
    void push(ScanEvent event) override;

    /// @brief queue済みeventをすべて取り出す。
    std::vector<ScanEvent> drain();

  private:
    std::mutex mutex_;
    std::deque<ScanEvent> events_;
};

} // namespace service::scan
