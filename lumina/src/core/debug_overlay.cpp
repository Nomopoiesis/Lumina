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

  row_count = 0;

  // Fixed-width fields against the monospace overlay font, so the digits
  // change in place instead of the whole string shifting sideways.
  AppendRow("FPS {:>5.1f}  {:>6.2f} ms", 1.0 / average_delta_time,
            average_delta_time * 1000.0);
  AppendRow("draws {:>7}", stats.draw_calls);

  if (stats.picked_entity == INVALID_ENTITY_ID) {
    AppendRow("picked      -");
  } else {
    AppendRow("picked {:>6}", stats.picked_entity);
  }

  // The phase total is summed from the same smoothed values the rows print, so
  // the unaccounted row is exactly what the rows above it fail to explain — a
  // gap computed from raw times would not match the digits on screen.
  f64 root_seconds = 0.0;
  f64 accounted_seconds = 0.0;
  for (size_t i = 0; i < stats.zone_samples.size(); ++i) {
    const auto &sample = stats.zone_samples[i];
    if (sample.name == nullptr) {
      continue;
    }
    if (sample.kind == profiling::ZoneKind::Frame) {
      root_seconds += stats.zone_seconds_ema[i];
    } else if (sample.kind == profiling::ZoneKind::Phase) {
      accounted_seconds += stats.zone_seconds_ema[i];
    }
  }

  // Emitted in kind order rather than zone id order, so the layout does not
  // depend on which call site happened to execute first.
  for (size_t i = 0; i < stats.zone_samples.size(); ++i) {
    const auto &sample = stats.zone_samples[i];
    if (sample.name != nullptr && sample.kind == profiling::ZoneKind::Frame) {
      AppendRow("{:<14}{:>6.2f} ms", sample.name,
                stats.zone_seconds_ema[i] * 1000.0);
    }
  }

  for (size_t i = 0; i < stats.zone_samples.size(); ++i) {
    const auto &sample = stats.zone_samples[i];
    if (sample.name != nullptr && sample.kind == profiling::ZoneKind::Phase) {
      AppendRow("  {:<12}{:>6.2f} ms {:>5}", sample.name,
                stats.zone_seconds_ema[i] * 1000.0, sample.call_count);
    }
  }

  // The number that indicts the job system rather than any one phase.
  if (root_seconds > 0.0) {
    AppendRow("  {:<12}{:>6.2f} ms", "unaccounted",
              (root_seconds - accounted_seconds) * 1000.0);
  }

  // Zones outside the frame root: render-thread work, and the update thread's
  // wait for it. Tagged rather than indented like a phase because they are near
  // this frame rather than part of it, and are excluded from the accounting
  // above. The prefix is deliberately neutral - not every detached zone is on
  // the render thread. Total field width matches the phase rows so the ms
  // column stays aligned.
  for (size_t i = 0; i < stats.zone_samples.size(); ++i) {
    const auto &sample = stats.zone_samples[i];
    if (sample.name != nullptr && sample.kind == profiling::ZoneKind::Detached) {
      AppendRow("[~] {:<10}{:>6.2f} ms {:>5}", sample.name,
                stats.zone_seconds_ema[i] * 1000.0, sample.call_count);
    }
  }
}

auto DebugOverlay::Draw(const DebugOverlayStats &stats) -> void {
  if (!visible) {
    return;
  }

  if (stats.total_time >= next_refresh_time ||
      stats.picked_entity != last_rendered_picked_entity) {
    RefreshRows(stats);
    last_rendered_picked_entity = stats.picked_entity;
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
