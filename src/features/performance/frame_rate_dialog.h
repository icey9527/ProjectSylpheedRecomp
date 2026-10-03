#pragma once

#include <rex/ui/imgui_dialog.h>

#include <atomic>
#include <functional>

namespace rex::ui {
class ImGuiDrawer;
}

namespace sylpheed::performance {

// Guest refresh rate selector, opened from the native Tools menu. The rate
// (video_mode_refresh_rate) requires a restart, so the dialog delegates the
// apply action instead of touching the runtime itself. The dialog is created
// once for the drawer's lifetime and only toggled by an atomic flag, mirroring
// the runtime info panel and the sensitivity dialog.
class FrameRateDialog final : public rex::ui::ImGuiDialog {
 public:
  FrameRateDialog(rex::ui::ImGuiDrawer* drawer, std::function<void(double)> apply);

  void SetApply(std::function<void(double)> apply) { apply_ = std::move(apply); }
  void SetVisible(bool visible);
  bool Visible() const { return visible_.load(std::memory_order_acquire); }

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  int Selected(double current) const;

  std::function<void(double)> apply_;
  std::atomic<bool> visible_{false};
  int selected_ = 1;
};

}  // namespace sylpheed::performance
