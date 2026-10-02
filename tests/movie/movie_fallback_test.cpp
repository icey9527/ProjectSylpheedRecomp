#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"
#include <rex/cvar.h>

#include <array>
#include <stdexcept>

REX_EXTERN(sub_821E9808);
REX_EXTERN(sub_821E9F50);
REX_EXTERN(sub_821EB8D0);
REX_EXTERN(sub_8256A518);
REX_EXTERN(sub_82569918);

namespace {
constexpr uint32_t impl = 0x1000;
bool fail_creation = false, unrelated_exception = false;
uint64_t creation_site = 0x821E9BCC;
int created = 0, updated = 0, completed = 0, status_calls = 0, pauses = 0;
int destroyed = 0;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
void Mode(bool skip, bool fallback) {
  rex::cvar::SetFlagByName("skip_movies", skip ? "true" : "false");
  rex::cvar::SetFlagByName("skip_failed_movies", fallback ? "true" : "false");
}
PPCContext Context() {
  PPCContext ctx{};
  ctx.r3.u32 = impl;
  ctx.r1.u32 = 0xE000;
  ctx.r30.u64 = 0x1122334455667788;
  ctx.f31.u64 = 0x0123456789ABCDEF;
  ctx.lr = 0x821EA690;
  return ctx;
}
}  // namespace

// Model the actual ABI boundary, deliberately clobbering PPC nonvolatile
// registers and the guest stack pointer before player creation. Host unwinding
// alone cannot restore them. Real game cleanup is verified separately at run.
REX_EXTERN(__imp__sub_821E9808) {
  if (unrelated_exception) throw std::runtime_error("unrelated");
  ctx.r1.u32 -= 400;
  ctx.r30.u64 = 42;
  ctx.f31.u64 = 7;
  ctx.lr = creation_site;
  sub_8256A518(ctx, base);
}
REX_EXTERN(__imp__sub_8256A518) {
  ++created;
  ctx.r3.u32 = fail_creation ? 0x80070002 : 0;
}
REX_EXTERN(__imp__sub_821E9F50) { ++updated; }
REX_EXTERN(__imp__sub_826A1760) {}
REX_EXTERN(__imp__sub_821EB8D0) { ++destroyed; }
REX_EXTERN(__imp__sub_82569918) {
  ++status_calls;
  ctx.r3.u32 = 0x80004003;
}
REX_EXTERN(sub_82819748) { ctx.r3.u32 = 0x2000; }
REX_EXTERN(sub_82818EB0) {
  Require(ctx.r4.u32 == 1, "startup must balance the exit vibration resumes");
  ++pauses;
}
REX_EXTERN(sub_821B0850) { ctx.r3.u32 = 0x3000; }
REX_EXTERN(sub_821A9F70) {
  Require(ctx.r4.u32 == 1, "preserve original startup sound pause");
}
REX_EXTERN(sub_821E8EE0) {
  ++completed;
  const auto object = ctx.r3.u32;
  ctx.r3.u32 = 0;
  ctx.r4.u32 = 0x4000;
  ctx.lr = 0x821E8F0C;
  sub_82569918(ctx, base);
  Require(ctx.r3.u32 == 0 && REX_LOAD_U32(0x4000) == 0 &&
          REX_LOAD_U32(0x4004) == 0, "completion must have a complete status pair");
  REX_STORE_U32(object + 24, 7);
}

int main() {
  alignas(64) std::array<uint8_t, 65536> memory{};
  auto* base = memory.data();
  Mode(true, true);
  auto ctx = Context();
  auto entry = ctx;
  sub_821E9808(ctx, base);
  Require(created == 0 && pauses == 4, "configured skip must not create a player");
  Require(ctx.r1.u64 == entry.r1.u64 && ctx.r30.u64 == entry.r30.u64 &&
          ctx.f31.u64 == entry.f31.u64 && ctx.lr == entry.lr, "restore the PPC entry frame");
  Require(REX_LOAD_U32(impl + 24) == 3, "defer completion until update");
  Require((REX_LOAD_U32(impl + 52) & 12) == 12,
          "suppress render-worker access before the deferred completion update");
  sub_821E9F50(ctx, base);
  Require(completed == 1 && status_calls == 0, "normal completion query must bypass absent player");
  ctx = Context();
  sub_821E9F50(ctx, base);
  Require(updated == 1, "completion marker must be consumed once");

  // Null players outside this one completion query retain original semantics.
  ctx = Context();
  ctx.r3.u32 = 0;
  ctx.r4.u32 = 0x4000;
  ctx.lr = 0x821E8F0C;
  sub_82569918(ctx, base);
  Require(status_calls == 1 && ctx.r3.u32 == 0x80004003, "no global null-player bypass");

  Mode(false, true);
  fail_creation = true;
  ctx = Context();
  sub_821E9808(ctx, base);
  Require(created == 1 && ctx.r1.u32 == 0xE000, "HRESULT failure must restore the guest stack");
  sub_821E9F50(ctx, base);
  Require(completed == 2, "failed creation must complete");

  fail_creation = false;
  ctx = Context();
  sub_821E9808(ctx, base);
  ctx = Context();
  sub_821E9F50(ctx, base);
  Require(updated == 2 && completed == 2, "successful player must use the original update");

  Mode(true, true);
  creation_site = 0xDEADBEEF;
  ctx = Context();
  sub_821E9808(ctx, base);
  Require(created == 3, "other creation sites must not be intercepted");
  creation_site = 0x821E9BCC;
  unrelated_exception = true;
  bool propagated = false;
  try { ctx = Context(); sub_821E9808(ctx, base); }
  catch (const std::runtime_error&) { propagated = true; }
  Require(propagated, "unrelated exceptions must propagate");
  unrelated_exception = false;

  // The startup scope must also be gone after exception propagation.
  ctx = Context(); ctx.lr = 0x821E9BCC;
  sub_8256A518(ctx, base);
  Require(created == 4, "no startup interception may escape its scope");
  Mode(false, false); fail_creation = true;
  ctx = Context(); sub_821E9808(ctx, base);
  Require(ctx.r3.u32 == 0x80070002 && ctx.r1.u32 == 0xE000 - 400,
          "disabled hooks must preserve original failure behavior");
  Mode(true, true);
  ctx = Context(); sub_821E9808(ctx, base);
  ctx = Context(); sub_821EB8D0(ctx, base);
  ctx = Context(); sub_821E9F50(ctx, base);
  Require(destroyed == 1 && updated == 3 && completed == 2,
          "cancellation must delegate destruction and remove stale completion markers");
  return 0;
}
