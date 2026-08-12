#pragma once

#include "lumina_types.hpp"

#include <functional>
#include <vector>

namespace lumina::renderer {

// For now it is not thread safe - but we can consider making it thread safe
// later
class DeviceRetirementQueue {
public:
  explicit DeviceRetirementQueue(u32 frames_in_flight)
      : max_frames_in_flight(frames_in_flight) {}

  DeviceRetirementQueue(const DeviceRetirementQueue &) = delete;
  auto operator=(const DeviceRetirementQueue &)
      -> DeviceRetirementQueue & = delete;

  DeviceRetirementQueue(DeviceRetirementQueue &&) noexcept = delete;
  auto operator=(DeviceRetirementQueue &&) noexcept
      -> DeviceRetirementQueue & = delete;

  ~DeviceRetirementQueue() noexcept = default;

  auto Retire(std::function<void()> callback) -> void;
  auto OnFrameComplete() -> void;
  auto Flush() -> void;

  [[nodiscard]] auto PendingCount() const noexcept -> size_t {
    return entries.size();
  }

private:
  struct RetirementEntry {
    std::function<void()> callback;
    u32 frames_remaining = 0;
  };

  std::vector<RetirementEntry> entries;
  std::vector<RetirementEntry> iterate_entries;
  u32 max_frames_in_flight = 0;
};

} // namespace lumina::renderer
