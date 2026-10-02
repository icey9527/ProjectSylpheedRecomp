#include "frame_metrics.h"

#include <algorithm>

namespace sylpheed::performance {

void FrameMetrics::Record(Clock::time_point now) {
  std::lock_guard lock(mutex_);
  times_[next_] = now;
  next_ = (next_ + 1) % capacity;
  size_ = std::min(size_ + 1, capacity);
  ++count_;
}

FrameSnapshot FrameMetrics::Snapshot(Clock::time_point now) const {
  std::lock_guard lock(mutex_);
  FrameSnapshot result;
  result.frame_count = count_;
  if (size_ < 2) return result;
  const auto latest = times_[(next_ + capacity - 1) % capacity];
  if (now < latest || now - latest > std::chrono::seconds(1)) return result;
  const auto cutoff = now - std::chrono::seconds(1);
  const unsigned first = (next_ + capacity - size_) % capacity;
  unsigned begin = 0;
  while (begin < size_ && times_[(first + begin) % capacity] < cutoff) ++begin;
  const unsigned samples = size_ - begin;
  if (samples < 2) return result;
  const double elapsed = std::chrono::duration<double>(
      latest - times_[(first + begin) % capacity]).count();
  if (elapsed <= 0) return result;
  result.recent = true;
  result.fps = (samples - 1) / elapsed;
  result.frame_time_ms = elapsed * 1000 / (samples - 1);
  const unsigned history_begin = size_ > 121 ? size_ - 121 : 0;
  for (unsigned i = history_begin + 1; i < size_; ++i) {
    const auto dt = times_[(first + i) % capacity] - times_[(first + i - 1) % capacity];
    result.history_ms[result.history_count++] =
        std::chrono::duration<float, std::milli>(dt).count();
  }
  return result;
}

FrameMetrics& Frames() {
  // The SDK can hard-exit a guest from a worker thread. Keep the collector
  // alive for process lifetime rather than destructing it under render workers.
  static auto* metrics = new FrameMetrics;
  return *metrics;
}

double CpuPercent(uint64_t cpu_ticks, uint64_t wall_ticks, unsigned processors) {
  if (!wall_ticks || !processors) return -1;
  return std::clamp(100.0 * double(cpu_ticks) / double(wall_ticks) / processors, 0.0, 100.0);
}

}  // namespace sylpheed::performance
