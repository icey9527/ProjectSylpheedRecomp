#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"
#include <rex/cvar.h>
#include <cstring>
#include <stdexcept>
#include <source_location>
#include <iostream>
#include <windows.h>

REX_EXTERN(sub_8280BF78);
REX_EXTERN(sub_82812268);
namespace {
int queries = 0, opens = 0;
bool throw_query = false, throw_open = false;
constexpr uint32_t original = 0x1000;
constexpr uint32_t title = 0x1100;
void Require(bool value, std::source_location location = std::source_location::current()) {
  if (!value) throw std::runtime_error("Bootstrap ABI mismatch at line " + std::to_string(location.line()));
}
PPCContext QueryContext(uint64_t caller) {
  PPCContext ctx{};
  ctx.lr = caller;
  ctx.r1.u32 = 0x2000;
  ctx.r3.u32 = 0x3000;
  ctx.r4.u32 = 0x1200;
  ctx.r30.u64 = 0x1122334455667788;
  return ctx;
}
}
extern "C" REX_FUNC(__imp__sub_8280BF78) {
  ++queries;
  if (throw_query) throw std::runtime_error("original query failure");
  ctx.r3.u64 = original;
  ctx.r5.u64 = 0xAABBCCDD;
  ctx.lr = 0x12345678;
}
extern "C" REX_FUNC(__imp__sub_82812268) {
  ++opens;
  if (throw_open) throw std::runtime_error("original open failure");
  ctx.r3.s64 = -7;
  ctx.r6.u64 = 0xCAFE;
}

int main() try {
  // Reserve address space, committing only two small ranges. This tests real
  // guest address arithmetic without allocating gigabytes of physical RAM.
  struct Arena {
    uint8_t* base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x82020000ull,
        MEM_RESERVE, PAGE_NOACCESS));
    ~Arena() { if (base) VirtualFree(base, 0, MEM_RELEASE); }
  } memory;
  Require(memory.base != nullptr);
  auto* base = memory.base;
  Require(VirtualAlloc(base, 0x3000, MEM_COMMIT, PAGE_READWRITE) != nullptr);
  Require(VirtualAlloc(base + 0x82010000, 0x1000, MEM_COMMIT, PAGE_READWRITE) != nullptr);
  std::memcpy(base + original, "GP_TEST", 8);
  std::memcpy(base + title, "GP_TITLE", 9);
  constexpr char path[] = "dat\\tables.pak+files.tbl";
  std::memcpy(base + 0x1200, path, sizeof(path));
  REX_STORE_U32(0x820101B8, title);
  rex::cvar::SetFlagByName("initial_game_part", "game");
  rex::cvar::SetFlagByName("trace_resource_bootstrap", "false");
  auto ctx = QueryContext(0x821A77E4);
  sub_8280BF78(ctx, base);
  Require(ctx.r3.u32 == original && ctx.r5.u64 == 0xAABBCCDD && ctx.lr == 0x12345678);
  rex::cvar::SetFlagByName("initial_game_part", "GP_TITLE");
  ctx = QueryContext(0x821A77E4);
  sub_8280BF78(ctx, base);
  Require(ctx.r3.u64 == title && ctx.r5.u64 == 0xAABBCCDD && ctx.r30.u64 == 0x1122334455667788);
  ctx = QueryContext(0x821A7948);
  sub_8280BF78(ctx, base);
  Require(ctx.r3.u32 == original); // Other GetStr uses retain their original value.
  rex::cvar::SetFlagByName("initial_game_part", "INVALID");
  ctx = QueryContext(0x821A77E4);
  sub_8280BF78(ctx, base);
  Require(ctx.r3.u32 == original);
  rex::cvar::SetFlagByName("initial_game_part", "GP_TITLE");
  REX_STORE_U32(0x820101B8, original);
  ctx = QueryContext(0x821A77E4);
  sub_8280BF78(ctx, base);
  Require(ctx.r3.u32 == original); // Mismatched image name table is refused.
  throw_query = true;
  bool caught = false;
  try { sub_8280BF78(ctx, base); } catch (const std::runtime_error&) { caught = true; }
  Require(caught && queries == 6);
  ctx = QueryContext(0x821A0230);
  rex::cvar::SetFlagByName("trace_resource_bootstrap", "true");
  sub_82812268(ctx, base);
  Require(ctx.r3.s64 == -7 && ctx.r6.u64 == 0xCAFE);
  throw_open = true;
  caught = false;
  try { sub_82812268(ctx, base); } catch (const std::runtime_error&) { caught = true; }
  Require(caught && opens == 2);
  return 0;
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
