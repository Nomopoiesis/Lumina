#include "file_watcher.hpp"

#include "platform/platform_common/platform_services.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace lumina::common {

namespace {

auto NowMs() -> u64 {
  return static_cast<u64>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

auto ReadMetadata(const std::string &path,
                  platform::common::FileMetadata &metadata) -> bool {
  return platform::common::PlatformServices::Instance().LuminaGetFileMetadata(
      path.c_str(), &metadata);
}

} // namespace

FileWatcher::FileWatcher(Config p_config) noexcept
    : config(p_config), last_slice_time(NowMs()) {}

auto FileWatcher::Watch(std::string p_path, OnFileChanged p_on_file_changed)
    -> void {
  WatchedFile file;
  file.path = std::move(p_path);
  file.on_file_changed = std::move(p_on_file_changed);

  platform::common::FileMetadata metadata;
  if (ReadMetadata(file.path, metadata)) {
    file.consumed_write_time = metadata.write_time_ns;
    file.consumed_size = metadata.size_bytes;
  }

  watched.push_back(std::move(file));
}

auto FileWatcher::ExamineFile(WatchedFile &file, u64 now_ms) -> bool {
  if (file.on_file_changed == nullptr) {
    return false;
  }

  platform::common::FileMetadata metadata;
  if (!ReadMetadata(file.path, metadata)) {
    // Unreadable at this instant — held open by a writer, or gone. Neither is
    // an answer, so any pending change keeps its place and the next slice
    // decides.
    return file.has_candidate;
  }

  if (metadata.write_time_ns == file.consumed_write_time &&
      metadata.size_bytes == file.consumed_size) {
    // Back at the version already handed over, so a writer reverted its own
    // change. Drop the candidate rather than reporting the round trip.
    file.has_candidate = false;
    return false;
  }

  if (!file.has_candidate ||
      metadata.write_time_ns != file.candidate_write_time ||
      metadata.size_bytes != file.candidate_size) {
    // First sighting of this change, or the writer is still working. Either
    // way the settle window starts from now: a file being written grows, so
    // the size moves between slices even across stretches where the timestamp
    // does not.
    file.candidate_write_time = metadata.write_time_ns;
    file.candidate_size = metadata.size_bytes;
    file.candidate_seen_at_ms = now_ms;
    file.has_candidate = true;
    return true;
  }

  if (now_ms - file.candidate_seen_at_ms < config.settle_ms) {
    return true;
  }

  if (!file.on_file_changed(file.path)) {
    // Rejected. The settle window has already elapsed, so the next slice
    // offers this same version again rather than losing the change.
    return true;
  }

  file.consumed_write_time = file.candidate_write_time;
  file.consumed_size = file.candidate_size;
  file.has_candidate = false;
  return false;
}

auto FileWatcher::PollAll() -> void {
  if (watched.empty()) {
    return;
  }

  const u64 now_ms = NowMs();
  if (now_ms - last_slice_time < config.slice_interval_ms) {
    return;
  }
  last_slice_time = now_ms;

  // Files with a change in flight, re-examined every slice instead of waiting
  // for the sweep to reach them again. Normally empty, so this costs nothing;
  // when it is not, the reloads it is about to trigger dwarf the queries.
  // Iterated backwards so a swap-and-pop removal cannot skip an entry.
  for (size_t i = candidates.size(); i > 0; --i) {
    const size_t candidate_index = i - 1;
    if (!ExamineFile(watched[candidates[candidate_index]], now_ms)) {
      candidates[candidate_index] = candidates.back();
      candidates.pop_back();
    }
  }

  // Round-robin sweep over everything else. Files already in the candidate
  // list still consume their turn: examining one twice in a slice would say
  // nothing new, and letting the loop skip past them would make the work per
  // slice depend on how many changes are in flight.
  const auto watched_count = static_cast<u32>(watched.size());
  const u32 slice_count = std::min(config.files_per_slice, watched_count);
  for (u32 examined = 0; examined < slice_count; ++examined) {
    const u32 index = sweep_index;
    sweep_index = (sweep_index + 1) % watched_count;

    auto &file = watched[index];
    if (file.has_candidate) {
      continue;
    }
    if (ExamineFile(file, now_ms)) {
      candidates.push_back(index);
    }
  }
}

} // namespace lumina::common
