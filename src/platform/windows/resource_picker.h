#pragma once
#include <windows.h>
#include <filesystem>
#include <optional>

namespace sylpheed {
std::optional<std::filesystem::path> PickResourceDirectory(HWND owner);
void ResourceMessage(HWND owner, const std::string& message, bool error);
// The folder dialog owns its operation; no App/Runtime pointers cross threads.
void BeginResourceDirectoryChange(const std::filesystem::path& config);
}
