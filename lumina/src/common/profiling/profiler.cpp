#include "profiler.hpp"

#include "lumina_check.hpp"

#include <atomic>

namespace lumina::profiling {

namespace {

struct ZoneAccumulator {
  std::atomic<u64> nanoseconds{0};
  std::atomic<u32> call_count{0};
};

struct ZoneRegistration {
  // Doubles as the published flag. Written with release after `kind`, so a
  // reader that acquires a non-null name is guaranteed to see the matching
  // kind — which is what makes the plain read of `kind` below race-free.
  std::atomic<const char *> name{nullptr};
  ZoneKind kind = ZoneKind::Phase;
};

// Namespace scope rather than function-local statics: both arrays are
// constant-initialized, so there is no initialization order to get wrong and no
// guard variable on the hot path.
std::array<ZoneAccumulator, MaxZones> accumulators{};
std::array<ZoneRegistration, MaxZones> registrations{};
std::atomic<u32> registered_count{0};

} // namespace

auto RegisterZone(const char *name, ZoneKind kind) -> u32 {
  const u32 id = registered_count.fetch_add(1, std::memory_order_relaxed);
  LUMINA_CHECK(id < MaxZones,
               "Profiler zone table is full; raise profiling::MaxZones");

  registrations[id].kind = kind;
  registrations[id].name.store(name, std::memory_order_release);
  return id;
}

ScopedZone::ScopedZone(u32 zone_id) noexcept
    : start(std::chrono::high_resolution_clock::now()), id(zone_id) {}

ScopedZone::~ScopedZone() noexcept {
  const auto end = std::chrono::high_resolution_clock::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

  // Relaxed on both: the accumulators order nothing, and the snapshot that
  // reads them only needs the values, not a view of anything they guard.
  auto &accumulator = accumulators[id];
  accumulator.nanoseconds.fetch_add(static_cast<u64>(elapsed),
                                    std::memory_order_relaxed);
  accumulator.call_count.fetch_add(1, std::memory_order_relaxed);
}

auto EndFrame(FrameProfile &out) -> void {
  const u32 claimed = registered_count.load(std::memory_order_relaxed);
  const u32 count = claimed < MaxZones ? claimed : MaxZones;

  out.zone_count = 0;
  out.total_seconds = 0.0;
  out.accounted_seconds = 0.0;

  for (u32 id = 0; id < count; ++id) {
    const char *name = registrations[id].name.load(std::memory_order_acquire);
    if (name == nullptr) {
      // Slot claimed by a RegisterZone that has not published yet. It shows up
      // next frame; missing a zone's very first frame is not worth
      // synchronising registration for.
      out.samples[id] = ZoneSample{};
      continue;
    }

    const u64 elapsed =
        accumulators[id].nanoseconds.exchange(0, std::memory_order_relaxed);
    const u32 calls =
        accumulators[id].call_count.exchange(0, std::memory_order_relaxed);
    const f64 seconds = static_cast<f64>(elapsed) * 1e-9;
    const ZoneKind kind = registrations[id].kind;

    out.samples[out.zone_count] = ZoneSample{
        .name = name,
        .seconds = seconds,
        .call_count = calls,
        .kind = kind,
    };
    ++out.zone_count;

    switch (kind) {
      case ZoneKind::Frame:
        out.total_seconds += seconds;
        break;
      case ZoneKind::Phase:
        out.accounted_seconds += seconds;
        break;
      case ZoneKind::Detached:
        break;
    }
  }
}

} // namespace lumina::profiling
