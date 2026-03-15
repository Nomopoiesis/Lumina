#pragma once

#include "lumina_types.hpp"

#include <chrono>

class Timer {
public:
  using Clock = std::chrono::high_resolution_clock;

  Timer() noexcept { Reset(); }

  auto Reset() noexcept -> void {
    start_time = Clock::now();
    last_time = start_time;
  }

  auto Tick() noexcept -> f64 {
    auto now = Clock::now();
    std::chrono::duration<f64, std::chrono::seconds::period> duration =
        now - last_time;
    delta_time = duration.count();
    total_time += delta_time;
    last_time = now;
    return delta_time;
  }

  [[nodiscard]] auto GetTotalTime() const noexcept -> f64 { return total_time; }
  [[nodiscard]] auto GetDeltaTime() const noexcept -> f64 { return delta_time; }

  [[nodiscard]] auto GetTotalTimeF() const noexcept -> f32 {
    return static_cast<f32>(total_time);
  }
  [[nodiscard]] auto GetDeltaTimeF() const noexcept -> f32 {
    return static_cast<f32>(delta_time);
  }

private:
  Clock::time_point start_time;
  Clock::time_point last_time;
  f64 delta_time = 0.0;
  f64 total_time = 0.0;
};