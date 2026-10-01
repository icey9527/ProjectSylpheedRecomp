#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace sylpheed {

inline std::string_view TrimConfig(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// The game uses an empty key as the default and #0xNN as integer keys.
// Read ASCII syntax without decoding or rewriting the original CP932 comments.
inline std::optional<unsigned> ReadDefaultLanguage(std::istream& input) {
  bool language_section = false;
  std::string line;
  std::optional<unsigned> result;
  while (std::getline(input, line)) {
    auto text = TrimConfig(line);
    text = TrimConfig(text.substr(0, text.find(';')));
    if (text.empty()) continue;
    if (text.front() == '[' && text.back() == ']') {
      language_section = TrimConfig(text.substr(1, text.size() - 2)) == "LANGUAGE";
      continue;
    }
    if (!language_section || text.front() != '=') continue;
    const auto value = TrimConfig(text.substr(1));
    constexpr std::string_view languages[] = {"eng", "jpn", "deu", "fra", "esp", "ita"};
    result.reset();
    for (unsigned i = 0; i < 6; ++i) {
      if (value == languages[i]) result = i + 1;
    }
  }
  return result;
}

inline std::optional<unsigned> ReadDefaultLanguage(const std::filesystem::path& root) {
  std::ifstream input(root / "config.ini", std::ios::binary);
  return ReadDefaultLanguage(input);
}

}  // namespace sylpheed
