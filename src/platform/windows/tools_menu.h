#pragma once

#include <windows.h>
#include <functional>

namespace sylpheed::performance {

class ToolsMenu {
 public:
  ToolsMenu(HWND window, std::function<void()> toggle,
            std::function<void()> change_resources,
            std::function<void()> toggle_missing_resources,
            std::function<bool()> missing_resources_checked);
  ~ToolsMenu();
  bool attached() const { return attached_; }
  void Update(bool checked, bool missing_resources_checked);

 private:
  static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
  HWND window_ = nullptr;
  HMENU menu_ = nullptr;
  HMENU popup_ = nullptr;
  bool attached_ = false;
  std::function<void()> toggle_;
  std::function<void()> change_resources_;
  std::function<void()> toggle_missing_resources_;
  std::function<bool()> missing_resources_checked_;
};

}  // namespace sylpheed::performance
