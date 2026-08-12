#include "device_retirement_queue.hpp"

namespace lumina::renderer {

auto DeviceRetirementQueue::Retire(std::function<void()> callback) -> void {
  entries.push_back({.callback = std::move(callback),
                     .frames_remaining = max_frames_in_flight});
}

auto DeviceRetirementQueue::OnFrameComplete() -> void {
  // Swapped out before the walk so a callback is free to call Retire: it
  // pushes into `entries`, which is empty here and not being iterated.
  std::swap(entries, iterate_entries);
  for (auto &entry : iterate_entries) {
    if (--entry.frames_remaining == 0) {
      entry.callback();
    } else {
      entries.push_back(std::move(entry));
    }
  }
  iterate_entries.clear();
}

auto DeviceRetirementQueue::Flush() -> void {
  while (!entries.empty()) {
    std::swap(entries, iterate_entries);
    for (auto &entry : iterate_entries) {
      entry.callback();
    }
    iterate_entries.clear();
  }
}

} // namespace lumina::renderer
