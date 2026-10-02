#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include <spdlog/sinks/base_sink.h>

namespace sylpheed {

// Converts real guest game-file ENOENT messages into one deduplicated UI notice.
// Cache misses and the expected loose files.tbl -> PAK fallback are ignored.
class MissingResourceNotifier final : public spdlog::sinks::base_sink<std::mutex> {
 public:
  using Callback = std::function<void(std::string)>;

  explicit MissingResourceNotifier(Callback callback);

 protected:
  void sink_it_(const spdlog::details::log_msg& message) override;
  void flush_() override {}

 private:
  Callback callback_;
  std::unordered_set<std::string> reported_;
};

}  // namespace sylpheed
