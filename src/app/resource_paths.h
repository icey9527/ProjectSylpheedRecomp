#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace sylpheed {
std::filesystem::path ResolveResourcePath(std::string_view value,
                                         const std::filesystem::path& exe_dir);
// Only startup essentials; optional movies and mission files are checked by
// their original loaders, not guessed from a full directory inventory.
std::string ResourceDirectoryError(const std::filesystem::path& root);
// Preserve all other TOML values and comments. Atomic replacement on Windows.
bool SaveResourceDirectory(const std::filesystem::path& config,
                           const std::filesystem::path& root, std::string& error);
// Writes the guest refresh rate (video_mode_refresh_rate) into the startup
// TOML with the same preserve-and-verify editing as SaveResourceDirectory.
bool SaveVideoModeRefreshRate(const std::filesystem::path& config, double rate,
                              std::string& error);
}  // namespace sylpheed
