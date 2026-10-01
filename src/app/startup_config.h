#pragma once

#include <rex/cvar.h>
#include <rex/rex_app.h>

#include <filesystem>
#include <string_view>

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
  DefaultFlag("mnk_mouse", "false");
  DefaultFlag("gpu_plugin", "xenos");
  DefaultFlag("allow_game_relative_writes", "false");
  DefaultFlag("keybind_a", "Return,Space");
  DefaultFlag("keybind_b", "Backspace");
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
}

}  // namespace sylpheed
