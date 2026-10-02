#include "performance_menu.h"

#include <commctrl.h>

namespace sylpheed::performance {
namespace {
constexpr UINT first_id = 0x7300;
constexpr UINT_PTR subclass_id = 0x53595050;
}

PerformanceMenu::PerformanceMenu(HWND window, std::function<void(unsigned)> toggle)
    : window_(window), toggle_(std::move(toggle)) {
  // Preserve any future SDK-owned menu instead of silently replacing it.
  if (!window_ || GetMenu(window_)) return;
  RECT client{};
  GetClientRect(window_, &client);
  menu_ = CreateMenu();
  popup_ = CreatePopupMenu();
  if (!menu_ || !popup_) return;
  const wchar_t* labels[] = {L"帧率与帧时间", L"进程 CPU 占用", L"内存占用"};
  for (unsigned i = 0; i < 3; ++i) {
    if (!AppendMenuW(popup_, MF_STRING, first_id + i, labels[i])) return;
  }
  if (!AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(popup_), L"显示")) return;
  if (!SetWindowSubclass(window_, WindowProc, subclass_id, reinterpret_cast<DWORD_PTR>(this))) return;
  if (!SetMenu(window_, menu_)) {
    RemoveWindowSubclass(window_, WindowProc, subclass_id);
    return;
  }
  attached_ = true;
  // Keep the game's client area unchanged when the native menu is added.
  RECT outer{0, 0, client.right, client.bottom};
  if (AdjustWindowRectExForDpi(&outer, static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE)),
                             TRUE, static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_EXSTYLE)),
                             GetDpiForWindow(window_))) {
    SetWindowPos(window_, nullptr, 0, 0, outer.right - outer.left, outer.bottom - outer.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }
  DrawMenuBar(window_);
}

PerformanceMenu::~PerformanceMenu() {
  if (window_ && IsWindow(window_) && attached_) {
    RemoveWindowSubclass(window_, WindowProc, subclass_id);
    SetMenu(window_, nullptr);
    DrawMenuBar(window_);
  }
  // Destroying the root also destroys its attached popup. Handle partial setup.
  if (menu_ && IsMenu(menu_)) {
    const bool owns_popup = GetMenuItemCount(menu_) > 0;
    DestroyMenu(menu_);
    if (!owns_popup && popup_ && IsMenu(popup_)) DestroyMenu(popup_);
  } else if (popup_ && IsMenu(popup_)) {
    DestroyMenu(popup_);
  }
}

void PerformanceMenu::Update(const std::array<bool, 3>& checked) {
  if (!attached_) return;
  for (unsigned i = 0; i < 3; ++i) {
    CheckMenuItem(popup_, first_id + i, MF_BYCOMMAND | (checked[i] ? MF_CHECKED : MF_UNCHECKED));
  }
}

LRESULT CALLBACK PerformanceMenu::WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp,
                                            UINT_PTR, DWORD_PTR data) {
  auto* self = reinterpret_cast<PerformanceMenu*>(data);
  if (message == WM_COMMAND && lp == 0 && HIWORD(wp) == 0 &&
      LOWORD(wp) >= first_id && LOWORD(wp) < first_id + 3) {
    self->toggle_(LOWORD(wp) - first_id);
    return 0;
  }
  if (message == WM_NCDESTROY) {
    RemoveWindowSubclass(window, WindowProc, subclass_id);
    self->window_ = nullptr;
    self->attached_ = false;
  }
  return DefSubclassProc(window, message, wp, lp);
}

}  // namespace sylpheed::performance
