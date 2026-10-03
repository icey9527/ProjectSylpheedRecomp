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
    for (int i = 0; i < int(std::size(kRates)); ++i) {
      char label[16];
      std::snprintf(label, sizeof(label), "%d Hz", kRates[i]);
      if (ImGui::RadioButton(label, selected_ == i)) selected_ = i;
      if (active == i) ImGui::SameLine(), ImGui::TextUnformatted("(current)");
    }
    ImGui::TextUnformatted(
        "The selected rate applies on the next restart; the game logic speed is");
    ImGui::TextUnformatted(
        "time-based, but higher rates are still experimental and may show issues.");
    if (ImGui::Button("Apply and restart")) {
      if (apply_) apply_(kRates[selected_]);
    }
  }
  ImGui::End();
}

}  // namespace sylpheed::performance
