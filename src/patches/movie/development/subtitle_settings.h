#pragma once
#include <cstdint>

namespace sylpheed::movie {
class SubtitleStartupScope {
 public:
  explicit SubtitleStartupScope(uint32_t impl);
  ~SubtitleStartupScope();
  SubtitleStartupScope(const SubtitleStartupScope&) = delete;
  SubtitleStartupScope& operator=(const SubtitleStartupScope&) = delete;
 private:
  uint32_t previous_;
};
}
