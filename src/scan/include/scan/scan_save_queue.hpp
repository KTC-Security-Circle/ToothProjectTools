#pragma once

#include "capture/capture_result.hpp"
#include "video/video_types.hpp"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace scan
{

struct ScanSaveJob
{
    std::string scan_id;
    int pattern_index{0};
    std::string pattern_kind;
    std::string expected_marker_state;
    std::string side;
    video::FrameSample sample;
    std::filesystem::path output_path;
};

struct ScanSaveFailure
{
    int pattern_index{0};
    std::string side;
    std::string error;
};

class ScanSaveQueue
{
  public:
    using SaveFunction = std::function<capture::CaptureResult(const video::FrameSample&,
                                                               const std::filesystem::path&)>;
    using CompletionFunction = std::function<void(const ScanSaveJob&)>;

    ScanSaveQueue(std::size_t worker_count, std::size_t capacity, SaveFunction save,
                  CompletionFunction completed = {});
    ~ScanSaveQueue();

    ScanSaveQueue(const ScanSaveQueue&) = delete;
    ScanSaveQueue& operator=(const ScanSaveQueue&) = delete;

    /// Queueが満杯の間だけ待つ。close済みまたは保存失敗後はfalseを返す。
    bool enqueue(ScanSaveJob job);
    /// 新規jobを閉じ、queue済み・実行中の保存が完了するまで待つ。
    void closeAndWait();
    std::optional<ScanSaveFailure> failure() const;
    std::size_t queuedCount() const;
    std::size_t capacity() const { return capacity_; }

  private:
    void workerLoop();

    const std::size_t capacity_;
    SaveFunction save_;
    CompletionFunction completed_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<ScanSaveJob> jobs_;
    std::vector<std::thread> workers_;
    std::optional<ScanSaveFailure> failure_;
    bool closed_{false};
};

} // namespace scan
