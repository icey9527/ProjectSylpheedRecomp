#pragma once

#include <windows.h>
#include <array>
#include <functional>

namespace sylpheed::performance {

class PerformanceMenu {
 public:
  PerformanceMenu(HWND window, std::function<void(unsigned)> toggle);
  ~PerformanceMenu();
  bool attached() const { return attached_; }
  void Update(const std::array<bool, 3>& checked);

 private:
  static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
  HWND window_ = nullptr;
  HMENU menu_ = nullptr;
  HMENU popup_ = nullptr;
  bool attached_ = false;
  std::function<void(unsigned)> toggle_;
};

}  // namespace sylpheed::performance
