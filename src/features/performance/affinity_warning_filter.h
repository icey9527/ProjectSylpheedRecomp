#pragma once

#include <spdlog/sinks/dist_sink.h>

namespace sylpheed::performance {

// Only throttles the SDK's exact repeated small-CPU warning. All other
// messages, including errors and other warnings, retain their normal path.
class AffinityWarningFilter final : public spdlog::sinks::dist_sink_mt {
 protected:
  void sink_it_(const spdlog::details::log_msg& message) override;

 private:
  bool started_ = false;
  spdlog::log_clock::time_point last_{};
  uint64_t suppressed_ = 0;
};

// Startup only, before guest/runtime threads are launched.
void InstallAffinityWarningFilter();

}  // namespace sylpheed::performance
