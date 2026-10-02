#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
extern "C" float roundevenf(float);

int main() {
  const int old_mode = std::fegetround();
  float (*volatile function)(float) = roundevenf;
  const std::array<std::array<float, 2>, 13> cases{{
      {0.0f, 0.0f}, {-0.0f, -0.0f}, {0.5f, 0.0f}, {-0.5f, -0.0f},
      {1.5f, 2.0f}, {-1.5f, -2.0f}, {2.5f, 2.0f}, {-2.5f, -2.0f},
      {3.5f, 4.0f}, {0.25f, 0.0f}, {-0.75f, -1.0f},
      {8388607.5f, 8388608.0f}, {-8388608.0f, -8388608.0f}}};
  for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
    if (std::fesetround(mode)) return 2;
    for (const auto& c : cases) {
      if (std::bit_cast<uint32_t>(function(c[0])) != std::bit_cast<uint32_t>(c[1])) {
        std::cerr << "rounding or signed zero mismatch\n";
        std::fesetround(old_mode); return 1;
      }
    }
    for (uint32_t bits : {0x7F800000u, 0xFF800000u, 0x7FC12345u})
      if (std::bit_cast<uint32_t>(function(std::bit_cast<float>(bits))) != bits) {
        std::fesetround(old_mode); return 1;
      }
    if (std::fegetround() != mode) return 1;
  }
  return std::fesetround(old_mode) ? 2 : 0;
}
