#include "resource_picker.h"
#include "app/resource_paths.h"
#include <shobjidl.h>
#include <shellapi.h>
#include <atomic>
#include <thread>
#include <vector>

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

namespace {
std::wstring QuoteArgument(std::wstring_view value) {
  std::wstring result = L"\"";
  for (const wchar_t c : value) {
    if (c == L'\"') result += L'\\';
    result += c;
  }
  result += L"\"";
  return result;
}

bool RestartHost(HWND owner) {
  wchar_t module[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(nullptr, module, ARRAYSIZE(module));
  if (!length || length >= ARRAYSIZE(module)) {
    ResourceMessage(owner, "无法确定宿主程序路径，资源目录已保存；请手动重启。", true);
    return false;
  }
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::wstring command = QuoteArgument(module);
  if (argv) {
    for (int i = 1; i < argc; ++i) {
      const std::wstring_view arg(argv[i]);
      // The new TOML value must win after a directory change. Keep unrelated
      // launch options such as GPU selection and isolated user-data paths.
      if (arg.starts_with(L"--game_data_root=") || arg.starts_with(L"--log_file=")) continue;
      command += L' ';
      command += QuoteArgument(arg);
    }
    LocalFree(argv);
  }
  std::vector<wchar_t> mutable_command(command.begin(), command.end());
  mutable_command.push_back(L'\0');
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  const std::filesystem::path working_dir = std::filesystem::path(module).parent_path();
  const BOOL started = CreateProcessW(module, mutable_command.data(), nullptr, nullptr, FALSE,
                                      0, nullptr, working_dir.c_str(), &startup, &process);
  if (!started) {
    ResourceMessage(owner, "资源目录已保存，但自动重启失败；请手动重启。", true);
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (owner && IsWindow(owner)) PostMessageW(owner, WM_CLOSE, 0, 0);
  return true;
}
}  // namespace

bool RestartHostForConfigChange(HWND owner) { return RestartHost(owner); }

void BeginResourceDirectoryChange(HWND owner, const std::filesystem::path& config) {
  static std::atomic<bool> active{false};
  if (active.exchange(true)) return;
  // Own a copy of the path and no game objects. A separate STA keeps the SDL
  // event/presentation loop pumping while the native folder dialog is open.
  try { std::thread([owner, config] {
    struct Reset { ~Reset() { active = false; } } reset;
    try {
      while (const auto root = PickResourceDirectory(nullptr)) {
        const auto problem = ResourceDirectoryError(*root);
        if (!problem.empty()) { ResourceMessage(nullptr, problem, true); continue; }
        std::string error;
        if (!SaveResourceDirectory(config, *root, error)) {
          ResourceMessage(owner, "无法保存资源目录：" + error, true);
          return;
        }
        RestartHost(owner);
        return;
      }
    } catch (...) { ResourceMessage(owner, "资源目录选择失败，原配置保持不变。", true); }
  }).detach(); }
  catch (...) {
    active = false;
    ResourceMessage(owner, "无法打开资源目录选择器，原配置保持不变。", true);
  }
}
}  // namespace sylpheed
