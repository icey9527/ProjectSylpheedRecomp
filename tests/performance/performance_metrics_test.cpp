#include "src/features/performance/frame_metrics.h"
#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

using namespace sylpheed::performance;
REX_EXTERN(sub_8235CD78);
namespace {
bool fail_swap = false;
int calls = 0;
void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
}
REX_EXTERN(__imp__sub_8235CD78) {
  ++calls;
  if (fail_swap) throw std::runtime_error("original swap failed");
  ctx.r3.u64 = 0x1122334455667788;
  ctx.lr = 0x82123456;
  ctx.f31.u64 = 0x0123456789abcdef;
}

int main() {
  using namespace std::chrono;
  const auto start = FrameMetrics::Clock::time_point{} + seconds(10);
  FrameMetrics metrics;
  Require(!metrics.Snapshot(start).recent, "no fabricated FPS before frames");
  for (int i = 0; i <= 60; ++i) metrics.Record(start + nanoseconds(16666666LL * i));
  const auto sample = metrics.Snapshot(start + seconds(1));
  Require(sample.recent && std::abs(sample.fps - 60) < .001 &&
          std::abs(sample.frame_time_ms - 1000.0 / 60) < .001, "known frame cadence");
  Require(sample.frame_count == 61 && sample.history_count == 60, "completed frame count and history");
  Require(metrics.Snapshot(start + seconds(1)).frame_count == 61,
          "UI reads must not count new game frames");
  Require(!metrics.Snapshot(start + seconds(3)).recent, "do not report stale FPS during a pause");

  FrameMetrics bounded;
  for (int i = 0; i < 1000; ++i) bounded.Record(start + milliseconds(10 * i));
  const auto wrap = bounded.Snapshot(start + milliseconds(9990));
  Require(wrap.frame_count == 1000 && wrap.history_count == 120 &&
          std::abs(wrap.fps - 100) < .001, "ring wrap preserves current rates and bounded history");
  bounded.Record(start + milliseconds(10490));
  const auto spike = bounded.Snapshot(start + milliseconds(10490));
  Require(spike.history_ms[spike.history_count - 1] == 500, "frame graph retains a stutter interval");
  Require(CpuPercent(10000000, 10000000, 4) == 25 &&
          CpuPercent(40000000, 10000000, 4) == 100, "CPU normalized to all logical cores");
  Require(CpuPercent(0, 0, 4) < 0 && CpuPercent(1, 1, 0) < 0, "invalid CPU samples unavailable");

  PPCContext ctx{};
  ctx.r1.u64 = 0x12345678;
  auto expected = ctx;
  __imp__sub_8235CD78(expected, nullptr);
  const auto before = Frames().Snapshot().frame_count;
  sub_8235CD78(ctx, nullptr);
  Require(calls == 2 && std::memcmp(&ctx, &expected, sizeof(ctx)) == 0,
          "instrumentation preserves the entire original PPC result");
  Require(Frames().Snapshot().frame_count == before + 1, "count only a completed original call");
  fail_swap = true;
  bool propagated = false;
  try { sub_8235CD78(ctx, nullptr); }
  catch (const std::runtime_error&) { propagated = true; }
  Require(propagated && Frames().Snapshot().frame_count == before + 1,
          "failed original calls propagate without a fake completed frame");
  return 0;
}
