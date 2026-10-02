// project_sylpheed - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/logging.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/ui/keybinds.h>
#include "startup_config.h"
#include "input_diagnostics.h"
#include "input/keyboard_keystroke_driver.h"
#include "features/performance/performance_display.h"
#include "features/performance/frame_metrics.h"
#include "features/performance/affinity_warning_filter.h"
#include "audio/host_audio_system.h"
#include "resource_paths.h"
#ifdef _WIN32
#include "platform/windows/resource_picker.h"
#endif

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
    auto_language_ = rex::cvar::GetFlagSource("user_language") == rex::cvar::Source::kDefault;
    sylpheed::ConfigureStartup(paths);
    startup_config_path_ = paths.config_path;
  }

  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)>) override {
    auto paths = defaults;
#ifdef _WIN32
    const auto owner = static_cast<HWND>(window()->GetNativeWindowHandle());
    for (;;) {
      const auto problem = sylpheed::ResourceDirectoryError(paths.game_data_root);
      if (problem.empty()) break;
      const auto utf8 = paths.game_data_root.generic_u8string();
      sylpheed::ResourceMessage(owner, problem + "\n当前目录：" +
          std::string(utf8.begin(), utf8.end()), true);
      const auto root = sylpheed::PickResourceDirectory(owner);
      if (!root) { app_context().RequestDeferredQuit(); return std::nullopt; }
      paths.game_data_root = *root;
      if (sylpheed::ResourceDirectoryError(*root).empty()) {
        std::string error;
        if (!sylpheed::SaveResourceDirectory(paths.config_path, *root, error))
          sylpheed::ResourceMessage(owner, "本次使用所选目录，但未能保存配置：" + error, true);
        if (auto_language_) {
          if (const auto language = sylpheed::ReadDefaultLanguage(*root))
            rex::cvar::SetFlagByName("user_language", std::to_string(*language));
        }
      }
    }
#endif
    return paths;
  }

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<ProjectSylpheedApp>(new ProjectSylpheedApp(ctx, "project_sylpheed",
        PPCImageConfig));
  }

  void OnLoadXexImage(std::string& xex_image) override {
    // Keep the game data root dedicated to game resources. The matching XEX2
    // image is a read-only file beside the host executable, exposed through a
    // separate VFS device without changing the official SDK.
    const auto image_root = rex::filesystem::GetExecutableFolder();
    const auto image_path = image_root / "BaseLib.dll";
    if (std::filesystem::is_regular_file(image_path)) {
      auto image_device = std::make_unique<rex::filesystem::HostPathDevice>(
          image_root.string(), image_root, true);
      if (image_device->Initialize()) {
        runtime()->file_system()->RegisterDevice(std::move(image_device));
        xex_image = image_path.string();
      } else {
        xex_image = image_path.string();
        REXLOG_ERROR("SYLPHEED_STAGE image_device_failed: {}", image_root.string());
      }
    } else {
      xex_image = image_path.string();
      REXLOG_ERROR("SYLPHEED_STAGE image_missing: {}", image_path.string());
    }
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
    if (performance_display_) performance_display_->AttachWindow(window(), [this] {
#ifdef _WIN32
      sylpheed::BeginResourceDirectoryChange(
          static_cast<HWND>(window()->GetNativeWindowHandle()), startup_config_path_);
#endif
    });
    SetGuestFrameStats([] {
      const auto sample = sylpheed::performance::Frames().Snapshot();
      return rex::ui::FrameStats{sample.frame_time_ms, sample.fps, sample.frame_count};
    });
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    performance_display_ = std::make_unique<sylpheed::performance::PerformanceDisplay>(drawer);
    // The SDK's default F7 callback destroys the dialog immediately from the
    // key event. ImGui may still be iterating the dialog vector in that frame,
    // so defer both creation and destruction to the next UI turn.
    rex::ui::UnregisterBind("bind_achievements");
    rex::ui::RegisterBind("bind_achievements", "F7", "Toggle achievements overlay",
                          [this] {
                            app_context().CallInUIThreadDeferred([this] {
                              if (achievements_overlay_safe_) {
                                achievements_overlay_safe_.reset();
                              } else {
                                achievements_overlay_safe_ = CreateAchievementsOverlay();
                              }
                            });
                          });
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
  std::unique_ptr<rex::ui::ImGuiDialog> achievements_overlay_safe_;
  std::unique_ptr<sylpheed::InputDiagnostics> input_diagnostics_;
  std::filesystem::path startup_config_path_;
  bool auto_language_ = true;
};
