#pragma once

#include "common/lumina_types.hpp"

#include <array>

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
};

class DebugOverlay {
public:
  // Emits the overlay's Clay elements for this frame. Must be called between
  // UISystem::BeginLayout and UISystem::EndLayout.
  auto Draw(const DebugOverlayStats &stats) -> void;

  auto Toggle() -> void { visible = !visible; }
  [[nodiscard]] auto IsVisible() const -> bool { return visible; }

private:
  static constexpr u32 MAX_ROWS = 4;
  static constexpr u32 ROW_CAPACITY = 64;

  auto RefreshRows(const DebugOverlayStats &stats) -> void;

  bool visible = true;

  // Wall-clock time at which the row text is next re-formatted. Values that
  // change every frame are unreadable, so the numbers update a few times a
  // second while the layout below is still emitted every frame.
  f64 next_refresh_time = 0.0;

  // One buffer per row rather than shared scratch: Clay stores the character
  // pointer and the render commands still dereference it during batch
  // building, so rows must not alias and must outlive EndLayout.
  std::array<std::array<char, ROW_CAPACITY>, MAX_ROWS> rows{};
  std::array<u32, MAX_ROWS> row_lengths{};
  u32 row_count = 0;
};

} // namespace lumina::core
