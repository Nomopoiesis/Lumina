#pragma once

#include "common/lumina_types.hpp"

#include <array>

namespace lumina::core {

static constexpr u32 SAMPLE_COUNT = 64;

class FrameStats {
public:
  FrameStats() noexcept = default;
  FrameStats(const FrameStats &) = delete;
  FrameStats(FrameStats &&) noexcept = delete;
  auto operator=(const FrameStats &) -> FrameStats & = delete;
  auto operator=(FrameStats &&) noexcept -> FrameStats & = delete;
  ~FrameStats() = default;

  auto Update(f64 delta_time) -> void {
    frame_deltas[index] = delta_time;
    ComputeAverageFrameDeltaTime();
    index = (index + 1) % SAMPLE_COUNT;
  }

  [[nodiscard]] auto GetAverageFrameDeltaTime() const -> f64 {
    return average_frame_delta_time;
  }

private:
  u32 index = 0;
  std::array<f64, SAMPLE_COUNT> frame_deltas{};
  f64 average_frame_delta_time = 0.0;

  auto ComputeAverageFrameDeltaTime() -> void {
    f64 sum = 0.0;
    for (const auto &delta : frame_deltas) {
      sum += delta;
    }
    average_frame_delta_time = sum / static_cast<f64>(SAMPLE_COUNT);
  }
};

} // namespace lumina::core
