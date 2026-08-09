#pragma once

#include "lumina_types.hpp"

#include <array>
#include <chrono>

// CPU zone profiling, active in Debug and Release alike.
//
// Unlike ASSERT, a profile scope has no side effects other than measurement, so
// compiling it away when LUMINA_PROFILING_ENABLED is 0 removes nothing but the
// measurement itself. Do not generalise that reasoning to macros whose argument
// does work — ASSERT discarding its condition under NDEBUG is exactly the bug
// this comment exists to keep separate.
//
// There is deliberately no thread_local anywhere below. A fiber parked in
// WaitForCounter can resume on a different worker, so a scope that began on
// thread A may end on thread B; per-thread accumulator buckets would corrupt
// both. ScopedZone holds its start time as a member instead, and the member
// travels with the fiber stack.

namespace lumina::profiling {

inline constexpr u32 MaxZones = 64;

// How a zone participates in the frame accounting the overlay prints.
enum class ZoneKind : u8 {
  // Nested inside the frame zone. Its time counts as accounted-for, so the
  // unaccounted row is what the phases do not explain.
  Phase = 0,
  // The frame root. Its duration is the denominator everything else is read
  // against. At most one zone should register as Frame.
  Frame = 1,
  // Measured but outside the frame root's span — render-thread zones, mainly.
  // Reported on its own line and excluded from the accounting.
  Detached = 2,
};

// Registers a zone name once and returns its dense id. Meant to be called from
// a function-local static, so it runs on first execution of each call site
// rather than on every entry. Terminates if more than MaxZones are registered.
auto RegisterZone(const char *name, ZoneKind kind = ZoneKind::Phase) -> u32;

class ScopedZone {
public:
  explicit ScopedZone(u32 zone_id) noexcept;
  ~ScopedZone() noexcept;
  ScopedZone(const ScopedZone &) = delete;
  auto operator=(const ScopedZone &) -> ScopedZone & = delete;
  ScopedZone(ScopedZone &&) = delete;
  auto operator=(ScopedZone &&) -> ScopedZone & = delete;

private:
  // Deliberately a member and not thread-local: see the header comment.
  std::chrono::high_resolution_clock::time_point start;
  u32 id;
};

struct ZoneSample {
  const char *name = nullptr;
  f64 seconds = 0.0;
  u32 call_count = 0;
  ZoneKind kind = ZoneKind::Phase;
};

struct FrameProfile {
  std::array<ZoneSample, MaxZones> samples{};
  u32 zone_count = 0;

  // Duration of the Frame-kind zone, and the sum of the Phase-kind zones.
  // The difference is the unaccounted time — the number that indicts the job
  // system rather than any one phase.
  f64 total_seconds = 0.0;
  f64 accounted_seconds = 0.0;
};

// Snapshots the accumulators into `out` and resets them to zero. Call once per
// frame from the update thread. Zones whose scopes are still open elsewhere
// contribute to whichever frame their destructor lands in, so a render-thread
// zone can be attributed to an adjacent frame — harmless at the overlay's
// refresh rate, and the reason Detached exists.
auto EndFrame(FrameProfile &out) -> void;

} // namespace lumina::profiling

// Two-level indirection so __LINE__ expands before it is pasted.
#define LUMINA_PROFILE_CONCAT_(a, b) a##b
#define LUMINA_PROFILE_CONCAT(a, b) LUMINA_PROFILE_CONCAT_(a, b)

#if LUMINA_PROFILING_ENABLED

#define LUMINA_PROFILE_ZONE_KIND(name, kind)                                   \
  static const u32 LUMINA_PROFILE_CONCAT(lumina_zone_id_, __LINE__) =          \
      ::lumina::profiling::RegisterZone((name), (kind));                       \
  const ::lumina::profiling::ScopedZone LUMINA_PROFILE_CONCAT(lumina_zone_,    \
                                                              __LINE__) {      \
    LUMINA_PROFILE_CONCAT(lumina_zone_id_, __LINE__)                           \
  }

#else

#define LUMINA_PROFILE_ZONE_KIND(name, kind) ((void)0)

#endif

// A phase inside the frame. The default and the one to reach for.
#define LUMINA_PROFILE_SCOPE(name)                                             \
  LUMINA_PROFILE_ZONE_KIND(name, ::lumina::profiling::ZoneKind::Phase)

// The frame root. Exactly one call site should use this.
#define LUMINA_PROFILE_FRAME(name)                                             \
  LUMINA_PROFILE_ZONE_KIND(name, ::lumina::profiling::ZoneKind::Frame)

// A zone that does not nest inside the frame root — anything on the render
// thread. Reported, but kept out of the accounted/unaccounted arithmetic.
#define LUMINA_PROFILE_SCOPE_DETACHED(name)                                    \
  LUMINA_PROFILE_ZONE_KIND(name, ::lumina::profiling::ZoneKind::Detached)

#define LUMINA_PROFILE_FUNCTION() LUMINA_PROFILE_SCOPE(__func__)
