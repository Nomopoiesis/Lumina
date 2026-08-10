#pragma once

#include "common/lumina_types.hpp"
#include "common/profiling/profiler.hpp"
#include "entity.hpp"

#include <array>
#include <format>
#include <span>

namespace lumina::core {

// Everything the overlay renders. The engine fills this in each frame; the
// overlay never reaches back into engine state, which keeps it independent of
// the frame executor and of Clay's headers.
struct DebugOverlayStats {
  f64 average_frame_delta_time = 0.0;
  f64 total_time = 0.0;

  // Scene draw calls from the last frame the render thread recorded, so this
  // trails the frame being updated. Fine for a diagnostic read a few times a
  // second, and it is the number the draw submission work is judged on.
  u32 draw_calls = 0;

  // Zone snapshot from the last completed frame, paired with the smoothed
  // seconds the rows actually print. Both are indexed by zone id, so element i
  // of one describes element i of the other, and entries with a null name are
  // unregistered slots to skip. Empty when profiling is compiled out.
  std::span<const profiling::ZoneSample> zone_samples;
  std::span<const f64> zone_seconds_ema;

  // INVALID_ENTITY_ID when the last pick hit nothing.
  EntityID picked_entity = INVALID_ENTITY_ID;
};

class DebugOverlay {
public:
  // Emits the overlay's Clay elements for this frame. Must be called between
  // UISystem::BeginLayout and UISystem::EndLayout.
  auto Draw(const DebugOverlayStats &stats) -> void;

  auto Toggle() -> void { visible = !visible; }
  [[nodiscard]] auto IsVisible() const -> bool { return visible; }

private:
  // Two fixed rows, the frame root, its phases, the unaccounted gap, and the
  // render-thread zones. Sized for the handful of phase zones that aggregate
  // counters are the right tool for rather than for profiling::MaxZones —
  // AppendRow drops anything past the end rather than overrunning.
  static constexpr u32 MAX_ROWS = 20;
  static constexpr u32 ROW_CAPACITY = 64;

  auto RefreshRows(const DebugOverlayStats &stats) -> void;

  // Formats one row into the next free buffer. Truncates at ROW_CAPACITY and
  // silently drops the row once MAX_ROWS is reached, so a zone table larger
  // than the overlay costs visibility, never memory safety.
  template <typename... Args>
  auto AppendRow(std::format_string<Args...> fmt, Args &&...args) -> void {
    if (row_count >= MAX_ROWS) {
      return;
    }
    auto &row = rows[row_count];
    const auto result = std::format_to_n(row.data(), ROW_CAPACITY, fmt,
                                         std::forward<Args>(args)...);
    row_lengths[row_count] = static_cast<u32>(result.out - row.data());
    ++row_count;
  }

  bool visible = true;

  // Wall-clock time at which the row text is next re-formatted. Values that
  // change every frame are unreadable, so the numbers update a few times a
  // second while the layout below is still emitted every frame.
  f64 next_refresh_time = 0.0;

  // The selection the rows were last formatted against. A click is a discrete
  // event rather than a drifting number, so it forces a refresh instead of
  // waiting up to REFRESH_INTERVAL_SECONDS — otherwise the overlay's own
  // refresh rate reads as latency in the pick.
  EntityID last_rendered_picked_entity = INVALID_ENTITY_ID;

  // One buffer per row rather than shared scratch: Clay stores the character
  // pointer and the render commands still dereference it during batch
  // building, so rows must not alias and must outlive EndLayout.
  std::array<std::array<char, ROW_CAPACITY>, MAX_ROWS> rows{};
  std::array<u32, MAX_ROWS> row_lengths{};
  u32 row_count = 0;
};

} // namespace lumina::core
