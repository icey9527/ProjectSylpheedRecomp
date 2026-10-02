#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace sylpheed::performance {

struct FrameSnapshot {
  double fps = 0;
  double frame_time_ms = 0;
  uint64_t frame_count = 0;
  bool recent = false;
  std::array<float, 120> history_ms{};
  unsigned history_count = 0;
};

// Completed game Swap calls, independent of the UI paint loop. Fixed storage;
// the render thread does not allocate, log, or query process statistics.
class FrameMetrics {
 public:
  using Clock = std::chrono::steady_clock;
  void Record(Clock::time_point now = Clock::now());
  FrameSnapshot Snapshot(Clock::time_point now = Clock::now()) const;

 private:
  static constexpr unsigned capacity = 240;
  mutable std::mutex mutex_;
  std::array<Clock::time_point, capacity> times_{};
  unsigned next_ = 0;
  unsigned size_ = 0;
  uint64_t count_ = 0;
};

FrameMetrics& Frames();

// CPU is normalized to the total capacity of all logical processors, matching
// Task Manager's process percentage. Return unavailable for invalid samples.
double CpuPercent(uint64_t cpu_ticks, uint64_t wall_ticks, unsigned processors);

}  // namespace sylpheed::performance
