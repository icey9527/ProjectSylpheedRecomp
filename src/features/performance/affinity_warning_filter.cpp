#include "affinity_warning_filter.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <chrono>
#include <string>

REXCVAR_DEFINE_BOOL(throttle_affinity_warning, true, "Performance",
                   "Limit repeated SDK small-CPU warnings to once per 10 seconds (restart required)");

namespace sylpheed::performance {
namespace {
constexpr auto warning = "Too few processor cores - scheduling will be wonky";
}

void AffinityWarningFilter::sink_it_(const spdlog::details::log_msg& message) {
  // dist_sink's mutex protects the count and forwarding, including concurrent
  // GPU interrupt threads. Preserve unrelated diagnostics without filtering.
  if (message.level != spdlog::level::warn || message.payload != warning) {
    spdlog::sinks::dist_sink_mt::sink_it_(message);
    return;
  }
  if (started_ && message.time >= last_ && message.time - last_ < std::chrono::seconds(10)) {
    ++suppressed_;
    return;
  }
  started_ = true;
  last_ = message.time;
  if (suppressed_) {
    const auto text = std::string(warning) + " (" + std::to_string(suppressed_) +
                      " repeated messages suppressed)";
    auto summary = message;
    summary.payload = text;
    spdlog::sinks::dist_sink_mt::sink_it_(summary);
    suppressed_ = 0;
  } else {
    spdlog::sinks::dist_sink_mt::sink_it_(message);
  }
}

void InstallAffinityWarningFilter() {
  if (!REXCVAR_GET(throttle_affinity_warning)) return;
  // Built-in categories are lazily registered. Force sys creation before the
  // first runtime warning instead of skipping a not-yet-registered category.
  const auto category = rex::log::sys();
  const auto logger = rex::GetLogger(category);
  if (!logger) return;
  const auto original = logger->sinks();
  const auto filter = std::make_shared<AffinityWarningFilter>();
  for (const auto& sink : original) filter->add_sink(sink);
  // Use SDK APIs so its periodic flushing cannot race with sink replacement.
  rex::AddSink(category, filter);
  for (const auto& sink : original) rex::RemoveSink(category, sink);
  REXLOG_INFO("SYLPHEED_PERF repeated small-CPU warning limited to once per 10 seconds");
}

}  // namespace sylpheed::performance
