#include "resource_picker.h"
#include "app/resource_paths.h"
#include <shobjidl.h>
#include <atomic>
#include <thread>

namespace sylpheed {
void ResourceMessage(HWND owner, const std::string& message, bool error) {
  const int count = MultiByteToWideChar(CP_UTF8, 0, message.data(), int(message.size()), nullptr, 0);
  std::wstring wide(count, 0);
  MultiByteToWideChar(CP_UTF8, 0, message.data(), int(message.size()), wide.data(), count);
  MessageBoxW(owner, wide.c_str(), L"Project Sylpheed", MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

std::optional<std::filesystem::path> PickResourceDirectory(HWND owner) {
  const auto init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  // The SDK UI thread may already use MTA; the dialog still works there.
  if (FAILED(init) && init != RPC_E_CHANGED_MODE) return std::nullopt;
  IFileOpenDialog* dialog = nullptr;
  std::optional<std::filesystem::path> result;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) {
    FILEOPENDIALOGOPTIONS options{};
    if (SUCCEEDED(dialog->GetOptions(&options))) {
      dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
      dialog->SetTitle(L"选择包含 config.ini 和 dat 的开发版资源目录");
      if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
          PWSTR path = nullptr;
          if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            result = std::filesystem::path(path);
            CoTaskMemFree(path);
          }
          item->Release();
        }
      }
    }
    dialog->Release();
  }
  if (SUCCEEDED(init)) CoUninitialize();
  return result;
}

void BeginResourceDirectoryChange(const std::filesystem::path& config) {
  static std::atomic<bool> active{false};
  if (active.exchange(true)) return;
  // Own a copy of the path and no game objects. A separate STA keeps the SDL
  // event/presentation loop pumping while the native folder dialog is open.
  try { std::thread([config] {
    struct Reset { ~Reset() { active = false; } } reset;
    try {
      while (const auto root = PickResourceDirectory(nullptr)) {
        const auto problem = ResourceDirectoryError(*root);
        if (!problem.empty()) { ResourceMessage(nullptr, problem, true); continue; }
        std::string error;
        if (!SaveResourceDirectory(config, *root, error)) {
          ResourceMessage(nullptr, "无法保存资源目录：" + error, true);
          return;
        }
        ResourceMessage(nullptr, "资源目录已保存。请退出并重新启动游戏后使用新目录。", false);
        return;
      }
    } catch (...) { ResourceMessage(nullptr, "资源目录选择失败，原配置保持不变。", true); }
  }).detach(); }
  catch (...) {
    active = false;
    ResourceMessage(nullptr, "无法打开资源目录选择器，原配置保持不变。", true);
  }
}
}  // namespace sylpheed
