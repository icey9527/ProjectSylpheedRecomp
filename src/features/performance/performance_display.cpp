#include "performance_display.h"
#include "frame_metrics.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/window.h>
#include <imgui.h>

#include <features/input/mouse_settings.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#include "platform/windows/tools_menu.h"
#include <psapi.h>
#endif

REXCVAR_DEFINE_BOOL(show_game_fps, false, "Performance", "Show game Swap rate and frame times");
REXCVAR_DEFINE_BOOL(show_process_cpu, false, "Performance", "Show process CPU usage");
REXCVAR_DEFINE_BOOL(show_process_memory, false, "Performance", "Show working set and private memory");
REXCVAR_DEFINE_BOOL(show_runtime_info, false, "Performance", "Show the collapsible runtime information panel");

namespace sylpheed::performance {
namespace {
struct ProcessSample {
  bool cpu_valid = false;
  bool memory_valid = false;
  double cpu = 0;
  double working_mib = 0;
  double private_mib = 0;
};

class ProcessSampler {
 public:
  const ProcessSample& Update() {
    const auto now = FrameMetrics::Clock::now();
    if (started_ && now - last_time_ < std::chrono::milliseconds(500)) return sample_;
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
      const auto ticks = [](FILETIME t) {
        return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
      };
      const uint64_t cpu_ticks = ticks(kernel) + ticks(user);
      if (cpu_started_ && cpu_ticks >= last_cpu_) {
        const auto wall_ticks = std::chrono::duration_cast<std::chrono::duration<uint64_t,
            std::ratio<1, 10000000>>>(now - last_cpu_time_).count();
        sample_.cpu = CpuPercent(cpu_ticks - last_cpu_, wall_ticks,
                                GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
        sample_.cpu_valid = sample_.cpu >= 0;
      } else {
        sample_.cpu_valid = false;
      }
      last_cpu_ = cpu_ticks;
      last_cpu_time_ = now;
      cpu_started_ = true;
    } else {
      sample_.cpu_valid = false;
      cpu_started_ = false;
    }
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    sample_.memory_valid = GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE;
    if (sample_.memory_valid) {
      sample_.working_mib = double(memory.WorkingSetSize) / (1024 * 1024);
      sample_.private_mib = double(memory.PrivateUsage) / (1024 * 1024);
    }
#endif
    last_time_ = now;
    started_ = true;
    return sample_;
  }

 private:
  ProcessSample sample_;
  FrameMetrics::Clock::time_point last_time_{};
  FrameMetrics::Clock::time_point last_cpu_time_{};
  uint64_t last_cpu_ = 0;
  bool started_ = false;
  bool cpu_started_ = false;
};

class PerformancePanel final : public rex::ui::ImGuiDialog {
 public:
  PerformancePanel(rex::ui::ImGuiDrawer* drawer, std::array<bool, 3> flags)
      : ImGuiDialog(drawer) { SetFlags(flags); }
  void SetFlags(std::array<bool, 3> flags) {
    uint8_t mask = 0;
    for (size_t i = 0; i < flags.size(); ++i) if (flags[i]) mask |= uint8_t(1u << i);
    flags_mask_.store(mask, std::memory_order_release);
    logged_sample_ = false;
  }

 protected:
  void OnDraw(ImGuiIO&) override {
    const uint8_t mask = flags_mask_.load(std::memory_order_acquire);
    if (!mask) return;
    const std::array<bool, 3> flags = {bool(mask & 1), bool(mask & 2), bool(mask & 4)};
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 12, viewport->WorkPos.y + 12),
        ImGuiCond_Always, ImVec2(1, 0));
    ImGui::SetNextWindowBgAlpha(0.75f);
    constexpr auto window_flags = ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
        // The overlay hugs the screen corner; dragging it by accident while
        // aiming with the mouse must not undock or resize it.
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    FrameSnapshot frames;
    ProcessSample process;
    if (flags[0]) frames = Frames().Snapshot();
    if (flags[1] || flags[2]) process = process_.Update();
    if (ImGui::Begin("Performance", nullptr, window_flags)) {
      if (flags[0]) {
        if (frames.recent && frames.history_count > 0) {
          ImGui::Text("Game: %.1f FPS  %.2f ms", frames.fps, frames.frame_time_ms);
          const auto peak = *std::max_element(frames.history_ms.begin(),
                                             frames.history_ms.begin() + frames.history_count);
          ImGui::PlotLines("##game_frames", frames.history_ms.data(), frames.history_count,
                           0, "Swap intervals (ms)", 0.0f, std::max(50.0f, peak * 1.1f),
                           ImVec2(270, 65));
        } else {
          ImGui::TextUnformatted("Game: waiting for recent frames");
        }
        ImGui::Text("Completed swaps: %llu", static_cast<unsigned long long>(frames.frame_count));
      }
      if (flags[1]) {
        if (process.cpu_valid) ImGui::Text("Process CPU: %.1f%%", process.cpu);
        else ImGui::TextUnformatted("Process CPU: sampling / unavailable");
      }
      if (flags[2]) {
        if (process.memory_valid) {
          ImGui::Text("Working set: %.1f MiB", process.working_mib);
          ImGui::Text("Private memory: %.1f MiB", process.private_mib);
        } else {
          ImGui::TextUnformatted("Process memory: unavailable");
        }
      }
    }
    ImGui::End();
    // One sample per selection, for acceptance evidence; no per-frame logs.
    if (!logged_sample_ && (!flags[0] || frames.recent) &&
        (!flags[1] || process.cpu_valid) && (!flags[2] || process.memory_valid)) {
      REXLOG_INFO("SYLPHEED_PERF sample fps={} ms={} swaps={} cpu={} working_mib={} private_mib={}",
                  flags[0] ? frames.fps : -1, flags[0] ? frames.frame_time_ms : -1,
                  frames.frame_count, flags[1] ? process.cpu : -1,
                  flags[2] ? process.working_mib : -1, flags[2] ? process.private_mib : -1);
      logged_sample_ = true;
    }
  }

 private:
  std::atomic<uint8_t> flags_mask_{0};
  ProcessSampler process_;
  bool logged_sample_ = false;
};
}

PerformanceDisplay::PerformanceDisplay(rex::ui::ImGuiDrawer* drawer) : drawer_(drawer) {
  // Migrate the previous independent switches only if the new option was not
  // explicitly configured. Keep the old cvars registered for existing TOMLs.
  if (rex::cvar::GetFlagSource("show_runtime_info") == rex::cvar::Source::kDefault &&
      (REXCVAR_GET(show_game_fps) || REXCVAR_GET(show_process_cpu) || REXCVAR_GET(show_process_memory)))
    rex::cvar::SetFlagByName("show_runtime_info", "true");
  // Created once for the drawer's lifetime; the Tools menu only toggles its
  // visibility, never creating or destroying ImGui state from the menu thread.
  mouse_settings_ = std::make_unique<sylpheed::input::MouseSettingsDialog>(drawer);
  Refresh();
}PerformanceDisplay::~PerformanceDisplay() = default;

void PerformanceDisplay::AttachWindow(rex::ui::Window* window, std::function<void()> change_resources) {
#ifdef _WIN32
  menu_ = std::make_unique<ToolsMenu>(static_cast<HWND>(window->GetNativeWindowHandle()),
                                      [this] { Toggle(); }, std::move(change_resources),
                                      [this] {
                                        if (mouse_settings_)
                                          mouse_settings_->SetVisible(
                                              !mouse_settings_->Visible());
                                      });
  if (!menu_->attached()) REXLOG_ERROR("SYLPHEED_PERF native menu could not be attached");
  else REXLOG_INFO("SYLPHEED_PERF native tools menu attached");
  menu_->Update(checked_[0]);
#else
  (void)window;
#endif
}

void PerformanceDisplay::Toggle() {
  if (!rex::cvar::SetFlagByName("show_runtime_info", checked_[0] ? "false" : "true")) return;
  Refresh();
  REXLOG_INFO("SYLPHEED_PERF options fps={} cpu={} memory={}", checked_[0], checked_[1], checked_[2]);
}

void PerformanceDisplay::Refresh() {
  checked_.fill(REXCVAR_GET(show_runtime_info));
#ifdef _WIN32
  if (menu_) menu_->Update(checked_[0]);
#endif
  // Keep the drawer alive for the lifetime of the app. The native menu runs
  // on the Win32/UI thread while ImGui drawers are painted by the renderer;
  // destroying a drawer from the menu callback races with an in-flight draw.
  if (!panel_) panel_ = std::make_unique<PerformancePanel>(drawer_, checked_);
  else static_cast<PerformancePanel*>(panel_.get())->SetFlags(checked_);
}

}  // namespace sylpheed::performance
