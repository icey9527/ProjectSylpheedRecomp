#include "resource_paths.h"

#include <toml++/toml.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace sylpheed {
std::filesystem::path ResolveResourcePath(std::string_view value,
                                         const std::filesystem::path& exe_dir) {
  std::filesystem::path path(std::u8string(value.begin(), value.end()));
  return (path.is_relative() ? exe_dir / path : path).lexically_normal();
}

std::string ResourceDirectoryError(const std::filesystem::path& root) {
  std::error_code ec;
  if (root.empty() || !std::filesystem::is_directory(root, ec))
    return "请选择完整游戏资源目录。";
  for (const auto* file : {"Xacalite_ScriptTeam.exe", "config.ini"}) {
    if (!std::filesystem::is_regular_file(root / file, ec))
      return std::string("所选目录缺少必需文件：") + file;
  }
  // Loose tables are optional: the guest owns PAK lookup and decompression.
  // A host filesystem existence check cannot see entries inside tables.pak.
  if (!std::filesystem::is_directory(root / "dat", ec))
    return "所选目录缺少资源目录：dat";
  return {};
}

namespace {
size_t Offset(std::string_view text, toml::source_position pos) {
  size_t at = text.starts_with("\xEF\xBB\xBF") ? 3 : 0;
  for (unsigned line = 1; line < pos.line; ++line) {
    at = text.find('\n', at);
    if (at == std::string_view::npos) throw std::runtime_error("Invalid TOML source line");
    ++at;
  }
  // toml++ columns count Unicode code points, not UTF-8 bytes.
  for (unsigned col = 1; col < pos.column; ++col) {
    if (at >= text.size()) throw std::runtime_error("Invalid TOML source column");
    ++at;
    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) ++at;
  }
  return at;
}
}

bool SaveResourceDirectory(const std::filesystem::path& config,
                           const std::filesystem::path& root, std::string& error) {
  std::filesystem::path temporary;
  try {
    std::string text;
    if (std::filesystem::exists(config)) {
      std::ifstream input(config, std::ios::binary);
      if (!input) throw std::runtime_error("Cannot read startup TOML");
      text.assign(std::istreambuf_iterator<char>(input), {});
    }
    auto table = toml::parse(text);
    const auto utf8 = root.lexically_normal().generic_u8string();
    std::ostringstream output;
    output << toml::toml_formatter{toml::value{std::string(utf8.begin(), utf8.end())}};
    if (auto* node = table.get("game_data_root")) {
      if (!node->is_string()) throw std::runtime_error("game_data_root must be a TOML string");
      const auto region = node->source();
      const auto begin = Offset(text, region.begin), end = Offset(text, region.end);
      text.replace(begin, end - begin, output.str());
    } else {
      text.insert(0, "game_data_root = " + output.str() + "\n");
    }
    // Validate the edited document and exact winning value before saving.
    if (toml::parse(text)["game_data_root"].value<std::string>() !=
        std::string(utf8.begin(), utf8.end())) throw std::runtime_error("TOML update verification failed");
#ifdef _WIN32
    wchar_t name[MAX_PATH];
    if (!GetTempFileNameW(config.parent_path().c_str(), L"psr", 0, name))
      throw std::runtime_error("Cannot create configuration temporary file");
    temporary = name;
#else
    temporary = config;
    temporary += ".tmp";
#endif
    {
      std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
      file.write(text.data(), text.size());
      file.close();
      if (!file) throw std::runtime_error("Cannot write startup TOML");
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), config.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot replace startup TOML");
#else
    std::filesystem::rename(temporary, config);
#endif
    return true;
  } catch (const std::exception& e) {
    error = e.what();
    std::error_code ignored;
    if (!temporary.empty()) std::filesystem::remove(temporary, ignored);
    return false;
  }
}
}  // namespace sylpheed
