#include "scan/scan_save_queue.hpp"

#include <stdexcept>
#include <utility>

namespace scan
{

ScanSaveQueue::ScanSaveQueue(std::size_t worker_count, std::size_t capacity, SaveFunction save,
                             CompletionFunction completed)
    : capacity_(capacity), save_(std::move(save)), completed_(std::move(completed))
{
    if (worker_count == 0 || capacity == 0 || !save_)
        throw std::invalid_argument("ScanSaveQueue requires workers, capacity, and save function");
    workers_.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index)
        workers_.emplace_back([this] { workerLoop(); });
}

ScanSaveQueue::~ScanSaveQueue()
{
    closeAndWait();
}

bool ScanSaveQueue::enqueue(ScanSaveJob job)
{
    std::unique_lock lock(mutex_);
    not_full_.wait(lock, [this] { return jobs_.size() < capacity_ || closed_ || failure_; });
    if (closed_ || failure_) return false;
    jobs_.push_back(std::move(job));
    not_empty_.notify_one();
    return true;
}

void ScanSaveQueue::closeAndWait()
{
    {
        std::lock_guard lock(mutex_);
        closed_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
    for (auto& worker : workers_)
        if (worker.joinable()) worker.join();
    workers_.clear();
}

std::optional<ScanSaveFailure> ScanSaveQueue::failure() const
{
    std::lock_guard lock(mutex_);
    return failure_;
}

std::size_t ScanSaveQueue::queuedCount() const
{
    std::lock_guard lock(mutex_);
    return jobs_.size();
}

void ScanSaveQueue::workerLoop()
{
    while (true)
    {
        ScanSaveJob job;
        {
            std::unique_lock lock(mutex_);
            not_empty_.wait(lock, [this] { return closed_ || !jobs_.empty(); });
            if (jobs_.empty()) return;
            job = std::move(jobs_.front());
            jobs_.pop_front();
            not_full_.notify_one();
        }

        capture::CaptureResult result;
        try
        {
            result = save_(job.sample, job.output_path);
        }
        catch (const std::exception& error)
        {
            result.error = capture::CaptureError{capture::CaptureErrorCode::FileWriteFailed, error.what()};
        }
        catch (...)
        {
            result.error = capture::CaptureError{capture::CaptureErrorCode::FileWriteFailed,
                                                 "unknown frame save exception"};
        }
        if (!result.ok)
        {
            std::lock_guard lock(mutex_);
            if (!failure_)
                failure_ = ScanSaveFailure{job.pattern_index, job.side,
                    result.error ? result.error->message : "frame sample save failed"};
            not_full_.notify_all();
            continue;
        }
        if (completed_) completed_(job);
    }
}

} // namespace scan
