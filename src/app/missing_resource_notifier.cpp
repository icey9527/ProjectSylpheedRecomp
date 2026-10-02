#include "missing_resource_notifier.h"

#include <functional>
#include <mutex>
#include <string_view>
#include <unordered_set>

namespace sylpheed {

MissingResourceNotifier::MissingResourceNotifier(Callback callback)
    : callback_(std::move(callback)) {}

void MissingResourceNotifier::sink_it_(const spdlog::details::log_msg& message) {
  const std::string_view payload(message.payload.data(), message.payload.size());
  if (message.level < spdlog::level::warn || payload.find("NtCreateFile") == std::string_view::npos ||
      payload.find("0xc000000f") == std::string_view::npos) {
    return;
  }

  constexpr std::string_view prefix = "path='game:\\";
  const auto begin = payload.find(prefix);
  if (begin == std::string_view::npos) return;
  const auto path_begin = begin + prefix.size();
  const auto path_end = payload.find('\'', path_begin);
  if (path_end == std::string_view::npos) return;
  const std::string path(payload.substr(path_begin, path_end - path_begin));
  if (path.ends_with("files.tbl")) return;
  if (!reported_.insert(path).second) return;
  if (callback_) callback_(path);
}

}  // namespace sylpheed
