// project_sylpheed - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/logging.h>
#include "startup_config.h"

class ProjectSylpheedApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

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
};
