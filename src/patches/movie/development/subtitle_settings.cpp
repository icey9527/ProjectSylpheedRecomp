#include "subtitle_settings.h"
#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"
#include <rex/cvar.h>

REXCVAR_DEFINE_STRING(movie_subtitles, "on", "Game",
                      "Movie subtitles: on, off, or game (use original profile)");
REX_EXTERN(__imp__sub_826A1760);  // AppFrame::XenonApp::GetDevice

namespace sylpheed::movie {
namespace { thread_local uint32_t startup_impl = 0; }
SubtitleStartupScope::SubtitleStartupScope(uint32_t impl) : previous_(startup_impl) { startup_impl = impl; }
SubtitleStartupScope::~SubtitleStartupScope() { startup_impl = previous_; }

extern "C" REX_FUNC(sub_826A1760) {
  // Exact call immediately after OnStart's original SystemData+76 -> Impl+108
  // copy at 0x821E9868, before font creation tests the byte. Change the movie
  // instance only, without writing a profile or changing SystemData/save data.
  if (startup_impl && ctx.lr == 0x821E9874) {
    const auto mode = REXCVAR_GET(movie_subtitles);
    if (mode == "on" || mode == "off") {
      const auto original = REX_LOAD_U8(startup_impl + 108) != 0;
      REX_STORE_U8(startup_impl + 108, mode == "on");
      REXLOG_INFO("SYLPHEED_SUBTITLE policy={} original_enabled={}", mode, original);
    } else if (mode != "game") {
      REXLOG_WARN("Unknown movie_subtitles='{}'; using original game profile", mode);
    }
  }
  __imp__sub_826A1760(ctx, base);
}
}  // namespace sylpheed::movie
