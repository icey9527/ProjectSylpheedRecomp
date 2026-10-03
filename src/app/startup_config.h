#pragma once

#include <rex/cvar.h>
#include <rex/rex_app.h>

#include <filesystem>
#include <fstream>
#include <string_view>
#include "game_config.h"

namespace sylpheed {

inline void DefaultFlag(std::string_view name, std::string_view value) {
  // Config, environment and CLI must retain their priority over host defaults.
  if (rex::cvar::GetFlagSource(name) == rex::cvar::Source::kDefault) {
    rex::cvar::SetFlagByName(name, value);
  }
}

inline void ConfigureStartup(rex::PathConfig& paths) {
  const auto exe_dir = paths.config_path.parent_path();
  // SDK 0.10 computes paths before loading TOML. Load it here and rebuild
  // paths from the winning values, preserving command-line overrides.
  if (std::filesystem::exists(paths.config_path)) {
    rex::cvar::LoadConfig(paths.config_path);
  }
  DefaultFlag("fullscreen", "false");
  DefaultFlag("window_width", "1280");
  DefaultFlag("window_height", "720");
  DefaultFlag("mnk_mode", "true");
  // Mouse look is the normal PC input path. The SDK already converts relative
  // mouse motion to the emulated right stick; keep this enabled by default so
  // a fresh build works without hand-editing the TOML file.
  DefaultFlag("mnk_mouse", "true");
  DefaultFlag("gpu_plugin", "xenos");
  const auto log_path = paths.config_path.parent_path() /
                        (paths.config_path.stem().string() + ".log");
  DefaultFlag("log_file", log_path.string());
  // ReXGlue's rotating sink appends when given an explicit path. Truncate the
  // single host log before logging initializes so every launch starts clean.
  {
    std::ofstream clear_log(log_path, std::ios::binary | std::ios::trunc);
  }
  DefaultFlag("allow_game_relative_writes", "false");
  DefaultFlag("keybind_a", "Return,Space");
  DefaultFlag("keybind_b", "Backspace");
  // Match the intended mouse controls while retaining the Xbox trigger model.
  DefaultFlag("keybind_left_trigger", "RMB");
  DefaultFlag("keybind_right_trigger", "LMB");
  DefaultFlag("keybind_start", "X");
  DefaultFlag("keybind_dpad_up", "Up");
  DefaultFlag("keybind_dpad_down", "Down");
  DefaultFlag("keybind_dpad_left", "Left");
  DefaultFlag("keybind_dpad_right", "Right");
  DefaultFlag("keybind_rstick_up", "Numpad8");
  DefaultFlag("keybind_rstick_down", "Numpad2");
  DefaultFlag("keybind_rstick_left", "Numpad4");
  DefaultFlag("keybind_rstick_right", "Numpad6");

  const auto resolve = [&](std::string_view name,
                           const std::filesystem::path& fallback) {
    const auto value = rex::cvar::GetFlagByName(name);
    if (value.empty()) return fallback;
    auto path = std::filesystem::path(std::u8string(value.begin(), value.end()));
    if (path.is_relative()) path = exe_dir / path;
    return path.lexically_normal();
  };
  paths.game_data_root = resolve("game_data_root", paths.game_data_root);
  paths.user_data_root = resolve("user_data_root", paths.user_data_root);
  paths.update_data_root = resolve("update_data_root", paths.update_data_root);
  paths.cache_root = resolve("cache_root", paths.user_data_root / "cache");
  paths.metadata_root = resolve("metadata_root", paths.metadata_root);
  // Keep the original resource priority: a loose GP_TEST directory wins;
  // otherwise a packaged GP_TEST.pak/.p00 pair is still a valid entry. Only
  // fall back to the title when neither representation exists. Explicit
  // initial_game_part remains the authority.
  const auto dat = paths.game_data_root / "dat";
  const bool has_loose_gp_test = std::filesystem::is_directory(dat / "GP_TEST");
  const bool has_packed_gp_test =
      std::filesystem::is_regular_file(dat / "GP_TEST.pak") &&
      std::filesystem::is_regular_file(dat / "GP_TEST.p00");
  if (rex::cvar::GetFlagSource("initial_game_part") == rex::cvar::Source::kDefault &&
      rex::cvar::GetFlagByName("initial_game_part") == "game" &&
      !has_loose_gp_test && !has_packed_gp_test) {
    rex::cvar::SetFlagByName("initial_game_part", "GP_TITLE");
    REXLOG_INFO("SYLPHEED_BOOTSTRAP no loose or packed GP_TEST table; defaulting initial_game_part=GP_TITLE");
  }
  // XTLGetLanguage reads this SDK flag via ExGetXConfigSetting. Keep explicit
  // TOML/environment/CLI overrides, otherwise use the game's own default.
  if (const auto language = ReadDefaultLanguage(paths.game_data_root)) {
    DefaultFlag("user_language", std::to_string(*language));
  }
}

}  // namespace sylpheed
