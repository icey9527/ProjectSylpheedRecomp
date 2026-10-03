#pragma once

#include <array>
#include <memory>
#include <functional>

namespace rex::ui {
class ImGuiDrawer;
class ImGuiDialog;
class Window;
}

namespace sylpheed::input {
class MouseSettingsDialog;
}

namespace sylpheed::performance {
class ToolsMenu;

class PerformanceDisplay {
 public:
  explicit PerformanceDisplay(rex::ui::ImGuiDrawer* drawer);
  ~PerformanceDisplay();
  void AttachWindow(rex::ui::Window* window, std::function<void()> change_resources = {});

 private:
  void Toggle();
  void Refresh();
  rex::ui::ImGuiDrawer* drawer_;
  std::array<bool, 3> checked_{};
  std::unique_ptr<rex::ui::ImGuiDialog> panel_;
  std::unique_ptr<sylpheed::input::MouseSettingsDialog> mouse_settings_;
#ifdef _WIN32
  std::unique_ptr<ToolsMenu> menu_;
#endif
};

}  // namespace sylpheed::performance
