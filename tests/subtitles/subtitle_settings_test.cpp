#include "patches/movie/development/subtitle_settings.h"
#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"
#include <rex/cvar.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

REX_EXTERN(sub_826A1760);
REX_EXTERN(__imp__sub_826A1760) { ctx.r11.u32 = 0x234; ctx.r3.u32 = 0x567; }
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
  try {
    std::array<uint8_t, 4096> memory;
    memory.fill(0xA5);
    auto* base = memory.data();
    constexpr uint32_t impl = 0x100;
    for (const auto* mode : {"on", "off", "game", "invalid"}) {
      rex::cvar::SetFlagByName("movie_subtitles", mode);
      for (uint8_t initial : {0, 1}) {
        REX_STORE_U8(impl + 108, initial);
        auto expected = memory;
        expected[impl + 108] = std::strcmp(mode, "on") == 0 ? 1 : std::strcmp(mode, "off") == 0 ? 0 : initial;
        PPCContext ctx{}, original{};
        ctx.lr = original.lr = 0x821E9874;
        __imp__sub_826A1760(original, base);
        { sylpheed::movie::SubtitleStartupScope scope(impl); sub_826A1760(ctx, base); }
        Require(std::memcmp(&ctx, &original, sizeof(ctx)) == 0, "preserve original getter PPC result/context");
        Require(memory == expected, "only the exact movie gate byte may change");
      }
    }
    rex::cvar::SetFlagByName("movie_subtitles", "on");
    REX_STORE_U8(impl + 108, 0);
    PPCContext ctx{}; ctx.lr = 0x821E9874;
    sub_826A1760(ctx, base);
    Require(REX_LOAD_U8(impl + 108) == 0, "no scope must not intercept another caller");
    { sylpheed::movie::SubtitleStartupScope scope(impl); ctx.lr = 0x821E8B7C; sub_826A1760(ctx, base); }
    Require(REX_LOAD_U8(impl + 108) == 0, "other movie device queries must retain original behavior");
    { sylpheed::movie::SubtitleStartupScope outer(impl);
      try { sylpheed::movie::SubtitleStartupScope inner(0x200); throw std::runtime_error("unwind"); } catch (...) {}
      ctx.lr = 0x821E9874; sub_826A1760(ctx, base);
    }
    Require(REX_LOAD_U8(impl + 108) == 1, "nested scope and exception restore outer movie instance");
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
