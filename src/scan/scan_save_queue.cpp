#include "scan/scan_save_queue.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace scan
{

ScanSaveQueue::ScanSaveQueue(std::size_t worker_count, std::size_t capacity, SaveFunction save,
                             CompletionFunction completed, TimingFunction timing)
    : capacity_(capacity), save_(std::move(save)), completed_(std::move(completed)), timing_(std::move(timing))
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
    return enqueueWithTiming(std::move(job)).accepted;
}

ScanSaveEnqueueTiming ScanSaveQueue::enqueueWithTiming(ScanSaveJob job)
{
    const auto begin = std::chrono::steady_clock::now();
    std::unique_lock lock(mutex_);
    const auto size_before = jobs_.size();
    not_full_.wait(lock, [this] { return jobs_.size() < capacity_ || closed_ || failure_; });
    const auto wait_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    if (closed_ || failure_) return {false, wait_ms, size_before, jobs_.size()};
    jobs_.push_back(std::move(job));
    maximum_queued_count_ = std::max(maximum_queued_count_, jobs_.size());
    const auto size_after = jobs_.size();
    not_empty_.notify_one();
    return {true, wait_ms, size_before, size_after};
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

std::size_t ScanSaveQueue::maximumQueuedCount() const
{
    std::lock_guard lock(mutex_);
    return maximum_queued_count_;
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
        const auto save_begin = std::chrono::steady_clock::now();
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
        const auto save_end = std::chrono::steady_clock::now();
        const auto save_ms = std::chrono::duration<double, std::milli>(save_end - save_begin).count();
        if (timing_) timing_(job, ScanSaveTiming{save_begin, save_end, save_ms, queuedCount()});
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
