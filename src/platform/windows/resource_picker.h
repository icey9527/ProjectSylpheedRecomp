#pragma once
#include <windows.h>
#include <filesystem>
#include <optional>

namespace sylpheed {
std::optional<std::filesystem::path> PickResourceDirectory(HWND owner);
void ResourceMessage(HWND owner, const std::string& message, bool error);
// The folder dialog owns its operation; no App/Runtime pointers cross threads.
// Saves the selected directory, launches a fresh copy of the host using the
// new configuration, and closes the current process after CreateProcessW
// succeeds. The HWND is only used for messages/PostMessage; no App object is
// captured by the worker thread.
void BeginResourceDirectoryChange(HWND owner, const std::filesystem::path& config);
}
