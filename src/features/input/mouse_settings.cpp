#include "mouse_settings.h"

#include <rex/cvar.h>
#include <rex/ui/window.h>
#include <imgui.h>

#include <algorithm>
#include <string>

// Defined in src/input/keyboard_keystroke_driver.cpp; same executable, so the
// typed storage accessor is the cheap read path.
REXCVAR_DECLARE(int32_t, mouse_look_scale);

namespace sylpheed::input {
namespace {
constexpr int32_t kMinScale = 1024;
constexpr int32_t kMaxScale = 65536;
constexpr int32_t kDefaultScale = 8192;

int32_t ClampedScale() {
  return std::clamp(REXCVAR_GET(mouse_look_scale), kMinScale, kMaxScale);
}
}  // namespace

MouseSettingsDialog::MouseSettingsDialog(rex::ui::ImGuiDrawer* drawer)
    : ImGuiDialog(drawer) {}

void MouseSettingsDialog::SetVisible(bool visible) {
  visible_.store(visible, std::memory_order_release);
}

void MouseSettingsDialog::OnDraw(ImGuiIO&) {
  if (!visible_.load(std::memory_order_acquire)) return;
  const auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12,
                                 viewport->WorkPos.y + 12),
                          ImGuiCond_FirstUseEver, ImVec2(1, 0));
  ImGui::SetNextWindowBgAlpha(0.9f);
  constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoSavedSettings;
  if (ImGui::Begin("Mouse sensitivity", nullptr, flags)) {
    int32_t scale = ClampedScale();
    if (ImGui::SliderInt("##mouse_look_scale", &scale, kMinScale, kMaxScale)) {
      rex::cvar::SetFlagByName("mouse_look_scale", std::to_string(scale));
    }
    ImGui::SameLine();
    ImGui::Text("%d", ClampedScale());
    ImGui::TextUnformatted(
        "Drag to adjust mouse-look strength; takes effect immediately.");
    ImGui::TextUnformatted(
        "Set mouse_look_scale in project_sylpheed.toml to keep it across restarts.");
    if (ImGui::Button("Reset to default")) {
      rex::cvar::SetFlagByName("mouse_look_scale", std::to_string(kDefaultScale));
    }
  }
  ImGui::End();
}

}  // namespace sylpheed::input
