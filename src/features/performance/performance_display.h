#pragma once

#include <array>
#include <memory>

namespace rex::ui {
class ImGuiDrawer;
class ImGuiDialog;
class Window;
}

namespace sylpheed::performance {
class PerformanceMenu;

class PerformanceDisplay {
 public:
  explicit PerformanceDisplay(rex::ui::ImGuiDrawer* drawer);
  ~PerformanceDisplay();
  void AttachWindow(rex::ui::Window* window);

 private:
  void Toggle(unsigned item);
  void Refresh();
  rex::ui::ImGuiDrawer* drawer_;
  std::array<bool, 3> checked_{};
  std::unique_ptr<rex::ui::ImGuiDialog> panel_;
#ifdef _WIN32
  std::unique_ptr<PerformanceMenu> menu_;
#endif
};

}  // namespace sylpheed::performance
