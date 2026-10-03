#pragma once

#include <rex/ui/imgui_dialog.h>

#include <atomic>

namespace rex::ui {
class ImGuiDrawer;
}

namespace sylpheed::input {

// Draggable mouse-look sensitivity control, opened from the native Tools
// menu. The dialog is created once for the drawer's lifetime and only toggles
// a visibility flag, mirroring the runtime info panel: ImGui dialogs must not
// be created or destroyed from the Win32 menu callback thread.
class MouseSettingsDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit MouseSettingsDialog(rex::ui::ImGuiDrawer* drawer);

  void SetVisible(bool visible);
  bool Visible() const { return visible_.load(std::memory_order_acquire); }

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  std::atomic<bool> visible_{false};
};

}  // namespace sylpheed::input
