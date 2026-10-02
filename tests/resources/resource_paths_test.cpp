#include "app/resource_paths.h"
#include <toml++/toml.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>

void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string Read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
int main() {
  const auto temp_root = std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
  const auto dir = temp_root /
      ("sylpheed-resource-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Require(std::filesystem::create_directory(dir), "unique test directory");
    std::filesystem::create_directory(dir / "dat");
    const auto config = dir / "test.toml";
    const auto root = dir / std::filesystem::path(u8"资源 '目录' 游戏");
    for (const auto* old_value : {"\"旧资源目录\"", "'''旧\n资源目录'''", "'old'"}) {
      const auto prefix = std::string("\xEF\xBB\xBF# existing comment\r\ngame_data_root = ");
      const auto suffix = std::string(" # keep inline comment\r\nkeybind_a = 'Return,Space'\r\n[logging]\r\nlevel = 'info'\r\n");
      { std::ofstream file(config, std::ios::binary); file << prefix << old_value << suffix; }
      std::string error;
      Require(sylpheed::SaveResourceDirectory(config, root, error), error.c_str());
      const auto result = Read(config);
      Require(result.starts_with(prefix) && result.ends_with(suffix), "preserve comments, CRLF and other values byte-for-byte");
      const auto utf8 = root.generic_u8string();
      Require(toml::parse(result)["game_data_root"].value<std::string>() == std::string(utf8.begin(), utf8.end()), "Unicode path round trip");
    }
    { std::ofstream file(config); file << "[logging]\nlevel='info'\n"; }
    std::string error;
    Require(sylpheed::SaveResourceDirectory(config, root, error), "insert root before existing section");
    { std::ofstream file(config); file << "invalid = [\n"; }
    const auto invalid = Read(config);
    Require(!sylpheed::SaveResourceDirectory(config, root, error) && Read(config) == invalid, "refuse invalid TOML without touching it");
    Require(sylpheed::ResolveResourcePath("game/../data", dir) == dir / "data", "relative path uses EXE directory");
    const auto root_utf8 = root.generic_u8string();
    Require(sylpheed::ResolveResourcePath(std::string(root_utf8.begin(), root_utf8.end()), dir) == root, "absolute path retained");
    Require(!sylpheed::ResourceDirectoryError(dir).empty(), "missing essential resources reported");
    for (const auto* path : {"Xacalite_ScriptTeam.exe", "config.ini", "dat/tables.pak", "dat/tables.p00"}) { std::ofstream file(dir / path); file << "fixture"; }
    Require(!std::filesystem::exists(dir / "dat/files.tbl"), "packed-only fixture has no loose table");
    Require(sylpheed::ResourceDirectoryError(dir).empty(), "packed resource layout must reach the original guest loader");
    std::filesystem::remove(dir / "dat/tables.pak");
    std::filesystem::remove(dir / "dat/tables.p00");
    { std::ofstream file(dir / "dat/files.tbl"); file << "fixture"; }
    Require(sylpheed::ResourceDirectoryError(dir).empty(), "loose resource layout accepted");
    std::filesystem::remove(dir / "dat/files.tbl");
    std::filesystem::remove(dir / "dat");
    Require(!sylpheed::ResourceDirectoryError(dir).empty(), "missing resource directory rejected");
    Require(std::filesystem::canonical(dir).parent_path() == temp_root &&
            std::filesystem::canonical(dir).filename() == dir.filename(), "cleanup stays in the exact test directory");
    std::filesystem::remove_all(dir);
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
