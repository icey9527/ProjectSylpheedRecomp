// project_sylpheed - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/logging.h>
#include "startup_config.h"
#include "input_diagnostics.h"
#include "input/keyboard_keystroke_driver.h"
#include "features/performance/performance_display.h"
#include "features/performance/frame_metrics.h"
#include "features/performance/affinity_warning_filter.h"
#include "audio/host_audio_system.h"

class ProjectSylpheedApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  void OnPostInitLogging() override {
    sylpheed::performance::InstallAffinityWarningFilter();
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    config.input_factory = REX_INPUT_BACKEND(sylpheed::input::CreateInputSystem);
    config.audio_factory = REX_AUDIO_BACKEND(sylpheed::audio::HostAudioSystem);
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    sylpheed::ConfigureStartup(paths);
  }

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<ProjectSylpheedApp>(new ProjectSylpheedApp(ctx, "project_sylpheed",
        PPCImageConfig));
  }

  void OnLoadXexImage(std::string& xex_image) override {
    // This host was generated for the development image, not retail default.xex.
    xex_image = "game:\\Xacalite_ScriptTeam.exe";
    REXLOG_INFO("SYLPHEED_STAGE image_selected: {}", xex_image);
  }

  void OnPostLoadXexImage() override {
    REXLOG_INFO("SYLPHEED_STAGE image_loaded");
  }

  void OnPostSetup() override {
    REXLOG_INFO("SYLPHEED_CONFIG user_language={} mnk_mode={}",
                rex::cvar::GetFlagByName("user_language"),
                rex::cvar::GetFlagByName("mnk_mode"));
    input_diagnostics_ = std::make_unique<sylpheed::InputDiagnostics>();
    window()->AddInputListener(input_diagnostics_.get(), 1);
    if (performance_display_) performance_display_->AttachWindow(window());
    SetGuestFrameStats([] {
      const auto sample = sylpheed::performance::Frames().Snapshot();
      return rex::ui::FrameStats{sample.frame_time_ms, sample.fps, sample.frame_count};
    });
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    performance_display_ = std::make_unique<sylpheed::performance::PerformanceDisplay>(drawer);
  }

  void OnShutdown() override {
    performance_display_.reset();
    if (input_diagnostics_) {
      window()->RemoveInputListener(input_diagnostics_.get());
      input_diagnostics_.reset();
    }
  }

  void OnPostLaunchModule(rex::system::XThread* thread) override {
    // SDK invokes this hook before resuming the guest thread. It is not a
    // claim that guest initialization or the first rendered frame succeeded.
    REXLOG_INFO("SYLPHEED_STAGE guest_thread_prepared");
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
 private:
  std::unique_ptr<sylpheed::performance::PerformanceDisplay> performance_display_;
  std::unique_ptr<sylpheed::InputDiagnostics> input_diagnostics_;
};
