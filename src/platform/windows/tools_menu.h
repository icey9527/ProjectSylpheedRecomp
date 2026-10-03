#pragma once

#include <windows.h>
#include <functional>

namespace sylpheed::performance {

class ToolsMenu {
 public:
  ToolsMenu(HWND window, std::function<void()> toggle,
            std::function<void()> change_resources,
            std::function<void()> mouse_settings);
  ~ToolsMenu();
  bool attached() const { return attached_; }
  void Update(bool checked);

 private:
  static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
  HWND window_ = nullptr;
  HMENU menu_ = nullptr;
  HMENU popup_ = nullptr;
  bool attached_ = false;
  std::function<void()> toggle_;
  std::function<void()> change_resources_;
  std::function<void()> mouse_settings_;
};

}  // namespace sylpheed::performance
