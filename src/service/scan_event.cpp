#include "service/scan_event.hpp"

#include <utility>

namespace service::scan
{

void ScanEventQueue::push(ScanEvent event)
{
    std::lock_guard lock(mutex_);
    events_.push_back(std::move(event));
}

std::vector<ScanEvent> ScanEventQueue::drain()
{
    std::deque<ScanEvent> pending;
    {
        std::lock_guard lock(mutex_);
        pending.swap(events_);
    }
    return {std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end())};
}

} // namespace service::scan
