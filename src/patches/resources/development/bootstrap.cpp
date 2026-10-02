#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"
#include <rex/cvar.h>
#include <array>
#include <string>
#include <string_view>

REXCVAR_DEFINE_STRING(initial_game_part, "game", "Game",
                     "Initial game part (game follows files.tbl, or an original GP_* name)");
REXCVAR_DEFINE_BOOL(trace_resource_bootstrap, false, "Game",
                   "Log startup table open results and the initial game part");

REX_EXTERN(__imp__sub_82812268); // TableA::Open(path)
REX_EXTERN(__imp__sub_8280BF78); // TableA::GetStr(key)

namespace {
constexpr uint64_t initial_part_return = 0x821A77E4;
constexpr uint32_t part_name_table = 0x820101B8;
constexpr std::array<std::string_view, 29> parts{
    "GP_TITLE", "GP_ADVERTISE_DEMO", "GP_SELECT_STORAGE", "GP_LOAD", "GP_SAVE",
    "GP_EXTRAS", "GP_MOVIE_THEATER", "GP_MISSION_SELECT", "GP_OPTIONS", "GP_MOVIE",
    "GP_BUNK", "GP_READY_ROOM", "GP_HANGAR", "GP_ARSENAL", "GP_PILOT_LOG",
    "GP_SYSTEM", "GP_DEMO", "GP_MAIN_GAME", "GP_SELECTOR", "GP_PAUSE_MENU",
    "GP_STAGE_CLEAR", "GP_MISSION_LOG", "GP_GAMEOVER", "GP_DEBRIEFING", "GP_DIALOG",
    "GP_TUTORIAL", "GP_CHALLENGE", "GP_LEADERBOARD", "GP_TEST"};

// Only read strings already passed to, or returned by, the original loader.
// Limits keep diagnostic output bounded; no temporary guest allocation is used.
std::string GuestString(uint32_t address, uint8_t* base) {
  if (!address) return "<null>";
  std::string result;
  for (uint32_t i = 0; i < 256 && address <= UINT32_MAX - i; ++i) {
    const auto value = REX_LOAD_U8(address + i);
    if (!value) break;
    result.push_back(static_cast<char>(value));
  }
  return result;
}
} // namespace

extern "C" REX_FUNC(sub_82812268) {
  const auto caller = ctx.lr;
  const bool trace = REXCVAR_GET(trace_resource_bootstrap) &&
                     caller >= 0x821A0080 && caller < 0x821A0430;
  const uint32_t table = ctx.r3.u32;
  const auto path = trace ? GuestString(ctx.r4.u32, base) : std::string{};
  __imp__sub_82812268(ctx, base);
  if (trace)
    REXLOG_INFO("SYLPHEED_BOOTSTRAP table=0x{:08X} path={} result={} caller=0x{:08X}",
                table, path, ctx.r3.s32, caller);
}

extern "C" REX_FUNC(sub_8280BF78) {
  const bool initial = ctx.lr == initial_part_return;
  __imp__sub_8280BF78(ctx, base);
  if (!initial) return;
  const auto selected = REXCVAR_GET(initial_game_part);
  const bool trace = REXCVAR_GET(trace_resource_bootstrap);
  const auto original = (trace || selected != "game") ? GuestString(ctx.r3.u32, base) : std::string{};
  if (selected != "game") {
    bool found = false;
    for (size_t i = 0; i < parts.size(); ++i) {
      if (selected != parts[i]) continue;
      const uint32_t pointer = REX_LOAD_U32(part_name_table + static_cast<uint32_t>(i) * 4);
      // Refuse a mismatched image/name layout; never inject a host pointer.
      if (GuestString(pointer, base) == selected) {
        ctx.r3.u64 = pointer;
        found = true;
      }
      break;
    }
    if (!found)
      REXLOG_ERROR("SYLPHEED_BOOTSTRAP invalid initial_game_part={} or image name table mismatch; keeping original entry", selected);
    else
      REXLOG_INFO("SYLPHEED_BOOTSTRAP initial_part original={} selected={}", original, selected);
  } else if (trace) {
    REXLOG_INFO("SYLPHEED_BOOTSTRAP initial_part original={} selected=game", original);
  }
}
