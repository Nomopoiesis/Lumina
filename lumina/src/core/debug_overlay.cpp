#include "debug_overlay.hpp"

#include <format>

#include "math/basic.hpp"

#include "clay.h"

namespace lumina::core {

static constexpr f64 REFRESH_INTERVAL_SECONDS = 0.1;

// Guards against a zero delta on the very first frames, where dividing to get
// a frame rate would otherwise produce infinity.
static constexpr f64 MIN_FRAME_DELTA_TIME = 0.0001;

static constexpr u16 OVERLAY_FONT_ID = 0;
static constexpr u16 OVERLAY_FONT_SIZE = 24;
static constexpr Clay_Color OVERLAY_TEXT_COLOR = {
    .r = 255, .g = 255, .b = 255, .a = 255};
static constexpr Clay_Color OVERLAY_BACKGROUND_COLOR = {
    .r = 0, .g = 0, .b = 0, .a = 200};
static constexpr f32 OVERLAY_MARGIN = 10.0F;
static constexpr Clay_Padding OVERLAY_PADDING = {
    .left = 8, .right = 8, .top = 4, .bottom = 4};

auto DebugOverlay::RefreshRows(const DebugOverlayStats &stats) -> void {
  const f64 average_delta_time =
      math::Max(stats.average_frame_delta_time, MIN_FRAME_DELTA_TIME);

  // Fixed-width fields against the monospace overlay font, so the digits
  // change in place instead of the whole string shifting sideways.
  auto &row = rows[0];
  const auto result =
      std::format_to_n(row.data(), ROW_CAPACITY, "FPS {:>5.1f}  {:>6.2f} ms",
                       1.0 / average_delta_time, average_delta_time * 1000.0);
  row_lengths[0] = static_cast<u32>(result.out - row.data());

  auto &draw_row = rows[1];
  const auto draw_result = std::format_to_n(
      draw_row.data(), ROW_CAPACITY, "draws {:>7}", stats.draw_calls);
  row_lengths[1] = static_cast<u32>(draw_result.out - draw_row.data());

  row_count = 2;
}

auto DebugOverlay::Draw(const DebugOverlayStats &stats) -> void {
  if (!visible) {
    return;
  }

  if (stats.total_time >= next_refresh_time) {
    RefreshRows(stats);
    next_refresh_time = stats.total_time + REFRESH_INTERVAL_SECONDS;
  }

  if (row_count == 0) {
    return;
  }

  // The panel rectangle is emitted before its text children, and the UI pass
  // draws commands in order without depth testing, so the text lands on top.
  CLAY({.id = CLAY_ID("DebugOverlay"),
        .layout = {.padding = OVERLAY_PADDING,
                   .layoutDirection = CLAY_TOP_TO_BOTTOM},
        .backgroundColor = OVERLAY_BACKGROUND_COLOR,
        .floating = {
            .offset = {OVERLAY_MARGIN, OVERLAY_MARGIN},
            .attachTo = CLAY_ATTACH_TO_ROOT,
        }}) {
    for (u32 i = 0; i < row_count; ++i) {
      // isStaticallyAllocated stays false: the buffer outlives the layout, but
      // its contents are rewritten on every refresh, so Clay must not retain
      // the text across frames.
      const auto text = Clay_String{
          .isStaticallyAllocated = false,
          .length = static_cast<i32>(row_lengths[i]),
          .chars = rows[i].data(),
      };
      CLAY_TEXT(text, CLAY_TEXT_CONFIG({.textColor = OVERLAY_TEXT_COLOR,
                                        .fontId = OVERLAY_FONT_ID,
                                        .fontSize = OVERLAY_FONT_SIZE}));
    }
  }
}

} // namespace lumina::core
