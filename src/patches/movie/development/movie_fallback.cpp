#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"

#include <rex/cvar.h>

#include <mutex>
#include <unordered_set>

REXCVAR_DEFINE_BOOL(skip_movies, false, "Game", "Skip movie playback");
REXCVAR_DEFINE_BOOL(skip_failed_movies, true, "Game",
                   "Complete movies whose player creation returns a failure");

REX_EXTERN(__imp__sub_821E9808);  // GamePart_Movie::Impl::OnStart
REX_EXTERN(__imp__sub_821E9F50);  // GamePart_Movie::Impl::OnUpdateFrame
REX_EXTERN(__imp__sub_821EB8D0);  // GamePart_Movie::Impl destructor
REX_EXTERN(__imp__sub_8256A518);  // XMediaCreateXmvPlayerFromFile
REX_EXTERN(__imp__sub_82569918);  // IXMediaXmvPlayer_GetStatus
REX_EXTERN(sub_821E8EE0);         // GamePart_Movie::Impl::ExitToPrevPart
REX_EXTERN(sub_82819748);         // BaseLib::InputDevice::Instance
REX_EXTERN(sub_82818EB0);         // BaseLib::GamePad::PauseVibration
REX_EXTERN(sub_821B0850);         // SoundManager::Instance
REX_EXTERN(sub_821A9F70);         // SoundManager::PauseGroupSE

namespace {
// Only these two explicit outcomes leave startup early. We do not catch guest
// exceptions, hardware faults, abort(), or errors from other game modules.
struct MovieSkipped {
  bool requested;
  uint32_t hresult;
};
thread_local bool in_movie_startup = false;
thread_local bool completing_skipped_movie = false;

struct Scope {
  bool& flag;
  bool previous;
  explicit Scope(bool& value) : flag(value), previous(value) { flag = true; }
  ~Scope() { flag = previous; }
};
std::mutex skipped_mutex;
std::unordered_set<uint32_t> skipped_movies;
}  // namespace

extern "C" REX_FUNC(sub_821E9808) {
  if (!REXCVAR_GET(skip_movies) && !REXCVAR_GET(skip_failed_movies)) {
    __imp__sub_821E9808(ctx, base);
    return;
  }
  const auto entry = ctx;
  const uint32_t impl = ctx.r3.u32;
  Scope startup(in_movie_startup);
  try {
    __imp__sub_821E9808(ctx, base);
  } catch (const MovieSkipped& skipped) {
    // The interception is exactly at 0x821E9BC8. All startup guest string
    // temporaries have already been destroyed there. Preserve its completed
    // scene/audio setup, but restore the PPC frame that host unwinding cannot
    // restore. No player is fabricated or published in impl + 84.
    ctx = entry;
    sub_82819748(ctx, base);
    const uint32_t pads = ctx.r3.u32;
    for (uint32_t i = 0; i < 4; ++i) {
      ctx.r3.u32 = pads + i * 80;
      ctx.r4.u32 = 1;
      sub_82818EB0(ctx, base);
    }
    sub_821B0850(ctx, base);
    ctx.r4.u32 = 1;
    sub_821A9F70(ctx, base);
    // Mirror the original startup tail at 0x821E9DA8..0x821E9DD0 so the
    // original exit routine can balance vibration and restore scene state.
    // OnScene runs on a render worker before the next update. Its existing
    // 0x4 flag must suppress drawing when there is no player; 0x8 requests
    // normal completion. Publish both before making the scene active.
    REX_STORE_U32(impl + 52, REX_LOAD_U32(impl + 52) | 4 | 8);
    REX_STORE_U32(impl + 24, 3);
    {
      std::lock_guard lock(skipped_mutex);
      skipped_movies.insert(impl);
    }
    REXLOG_WARN("SYLPHEED_MOVIE skipped impl=0x{:08X} reason={} hresult=0x{:08X}",
                impl, skipped.requested ? "configured_skip" : "creation_failed", skipped.hresult);
    ctx = entry;
  }
}

extern "C" REX_FUNC(sub_8256A518) {
  // Leave XMedia users other than GamePart_Movie's exact creation site alone.
  const bool movie = in_movie_startup && ctx.lr == 0x821E9BCC;
  if (movie && REXCVAR_GET(skip_movies)) {
    throw MovieSkipped{true, 0};
  }
  __imp__sub_8256A518(ctx, base);
  if (movie && REXCVAR_GET(skip_failed_movies) && ctx.r3.s32 < 0) {
    throw MovieSkipped{false, ctx.r3.u32};
  }
}

extern "C" REX_FUNC(sub_821E9F50) {
  const uint32_t impl = ctx.r3.u32;
  bool skipped;
  {
    std::lock_guard lock(skipped_mutex);
    skipped = skipped_movies.erase(impl) != 0;
  }
  if (!skipped) {
    __imp__sub_821E9F50(ctx, base);
    return;
  }
  // Defer completion to the next update, like the original ended/skip path.
  // This releases the prepared UI/tables, restores the render loop and movie
  // sound mode, resumes vibration, and sends ReturnToPrevPart normally.
  Scope completion(completing_skipped_movie);
  sub_821E8EE0(ctx, base);
  REXLOG_INFO("SYLPHEED_MOVIE completed impl=0x{:08X} state={}",
              impl, REX_LOAD_U32(impl + 24));
}

extern "C" REX_FUNC(sub_82569918) {
  if (completing_skipped_movie && ctx.lr == 0x821E8F0C && ctx.r3.u32 == 0) {
    // No player was created. Status zero follows ExitToPrevPart's existing
    // completion branch. Limit this to its exact query, not all null players.
    const auto output = ctx.r4.u32;
    REX_STORE_U32(output, 0);
    REX_STORE_U32(output + 4, 0);
    ctx.r3.u32 = 0;
    return;
  }
  __imp__sub_82569918(ctx, base);
}

extern "C" REX_FUNC(sub_821EB8D0) {
  // Scene cancellation may destroy the movie before its first update.
  {
    std::lock_guard lock(skipped_mutex);
    skipped_movies.erase(ctx.r3.u32);
  }
  __imp__sub_821EB8D0(ctx, base);
}
