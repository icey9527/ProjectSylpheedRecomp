#include <cmath>
#include <cstdint>

// The generated VMX code uses Clang's roundevenf lowering. This Windows CRT
// does not provide the symbol. Round ties to even without changing thread fenv.
extern "C" float roundevenf(float value) {
  const float magnitude = std::fabs(value);
  if (!(magnitude < 8388608.0f)) return value; // integers, infinity, and NaN
  const auto whole = static_cast<uint32_t>(magnitude);
  const float fraction = magnitude - static_cast<float>(whole);
  const auto rounded = whole + (fraction > 0.5f || (fraction == 0.5f && (whole & 1)));
  return std::copysign(static_cast<float>(rounded), value);
}
