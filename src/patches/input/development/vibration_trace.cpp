#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"

#include <rex/cvar.h>

#include <array>
#include <mutex>

REXCVAR_DEFINE_BOOL(trace_vibration, false, "Input",
                   "Log changes to guest vibration commands for diagnosis");

// XInputSetState, xapilib:xinpapi.obj, MAP/PDB exact name and location.
// Strong symbol replaces the generated weak alias; __imp__ remains original.
REX_EXTERN(__imp__sub_82338D40);
extern "C" REX_FUNC(sub_82338D40) {
  if (!REXCVAR_GET(trace_vibration)) {
    __imp__sub_82338D40(ctx, base);
    return;
  }
  const auto user = ctx.r3.u32;
  const auto pointer = ctx.r4.u32;
  const auto caller = ctx.lr;
  if (!pointer || user >= 4) {
    __imp__sub_82338D40(ctx, base);
    return;
  }
  const uint16_t left = REX_LOAD_U16(pointer);
  const uint16_t right = REX_LOAD_U16(pointer + 2);
  __imp__sub_82338D40(ctx, base);
  const auto result = ctx.r3.u32;
  struct State {
    bool seen = false;
    uint16_t left = 0, right = 0;
    uint32_t result = 0;
    uint64_t repeats = 0;
  };
  static std::mutex mutex;
  static std::array<State, 4> states;
  std::lock_guard lock(mutex);
  auto& state = states[user];
  if (state.seen && state.left == left && state.right == right && state.result == result) {
    ++state.repeats;
    return;
  }
  REXLOG_INFO("SYLPHEED_VIBRATION user={} left={} right={} result={} caller=0x{:08X} previous_repeats={}",
              user, left, right, result, caller, state.repeats);
  state = {true, left, right, result, 0};
}
