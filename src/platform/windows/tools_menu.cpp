#include "tools_menu.h"

#include <commctrl.h>

namespace sylpheed::performance {
namespace {
constexpr UINT first_id = 0x7300;
constexpr UINT_PTR subclass_id = 0x53595050;
}

ToolsMenu::ToolsMenu(HWND window, std::function<void()> toggle,
                                 std::function<void()> change_resources)
    : window_(window),
      toggle_(std::move(toggle)),
      change_resources_(std::move(change_resources)) {
  // Preserve any future SDK-owned menu instead of silently replacing it.
  if (!window_ || GetMenu(window_)) return;
  RECT client{};
  GetClientRect(window_, &client);
  menu_ = CreateMenu();
  popup_ = CreatePopupMenu();
  if (!menu_ || !popup_) return;
  if (!AppendMenuW(popup_, MF_STRING, first_id, L"运行信息")) return;
  if (!AppendMenuW(popup_, MF_SEPARATOR, 0, nullptr)) return;
  if (!AppendMenuW(popup_, MF_STRING, first_id + 1, L"更改资源目录…")) return;
  if (!AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(popup_), L"工具")) return;
  // Keep SDL's event / presentation loop running while the menu is open.
  // A modal Win32 menu otherwise blocks that loop while audio keeps running.
  MENUINFO info{};
  info.cbSize = sizeof(info);
  info.fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS;
  info.dwStyle = MNS_MODELESS;
  if (!SetMenuInfo(menu_, &info)) return;
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

ToolsMenu::~ToolsMenu() {
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

void ToolsMenu::Update(bool checked) {
  if (!attached_) return;
  CheckMenuItem(popup_, first_id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
}

LRESULT CALLBACK ToolsMenu::WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp,
                                            UINT_PTR, DWORD_PTR data) {
  auto* self = reinterpret_cast<ToolsMenu*>(data);
  if (message == WM_COMMAND && lp == 0 && HIWORD(wp) == 0 &&
      LOWORD(wp) >= first_id && LOWORD(wp) < first_id + 2) {
    if (LOWORD(wp) == first_id) {
      self->toggle_();
    } else if (LOWORD(wp) == first_id + 1) {
      if (self->change_resources_) self->change_resources_();
    }
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
