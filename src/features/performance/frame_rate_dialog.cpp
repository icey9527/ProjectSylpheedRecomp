#include "frame_rate_dialog.h"

#include <rex/cvar.h>
#include <rex/ui/window.h>
#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <cstddef>

namespace sylpheed::performance {
namespace {
// Only the tested ladder; arbitrary values stay available through the TOML.
constexpr int kRates[] = {30, 60, 90, 120};
}  // namespace

FrameRateDialog::FrameRateDialog(rex::ui::ImGuiDrawer* drawer,
                                 std::function<void(double)> apply)
    : ImGuiDialog(drawer), apply_(std::move(apply)) {}

void FrameRateDialog::SetVisible(bool visible) {
  visible_.store(visible, std::memory_order_release);
}

int FrameRateDialog::Selected(double current) const {
  for (int i = 0; i < int(std::size(kRates)); ++i) {
    if (std::abs(current - kRates[i]) < 0.5) return i;
  }
  return -1;
}

void FrameRateDialog::OnDraw(ImGuiIO&) {
  if (!visible_.load(std::memory_order_acquire)) return;
  const auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12,
                                 viewport->WorkPos.y + 12),
                          ImGuiCond_FirstUseEver, ImVec2(1, 0));
  ImGui::SetNextWindowBgAlpha(0.9f);
  constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoSavedSettings;
  if (ImGui::Begin("Frame rate", nullptr, flags)) {
    const double current = rex::cvar::Query<double>("video_mode_refresh_rate");
    const int active = Selected(current);
    ImGui::Text("Effective now: %.0f Hz", current);
    ImGui::Separator();
    for (int i = 0; i < int(std::size(kRates)); ++i) {
      char label[16];
      std::snprintf(label, sizeof(label), "%d Hz", kRates[i]);
      const bool active_selected = i == active;
      if (active_selected) ImGui::BeginDisabled();
      if (ImGui::RadioButton(label, selected_ == i)) selected_ = i;
      if (active_selected) ImGui::EndDisabled();
      if (active_selected) ImGui::SameLine(), ImGui::TextUnformatted("<- in effect");
    }
    ImGui::TextUnformatted(
        "Applying restarts the host; the selection above is pending until then.");
    ImGui::TextUnformatted(
        "Higher rates are experimental: time-based logic is unaffected, but");
    ImGui::TextUnformatted("frame-dependent behavior may show issues.");
    char button[48];
    std::snprintf(button, sizeof(button), "Apply %d Hz and restart", kRates[selected_]);
    if (ImGui::Button(button)) {
      if (apply_) apply_(kRates[selected_]);
    }
  }
  ImGui::End();
}

}  // namespace sylpheed::performance
