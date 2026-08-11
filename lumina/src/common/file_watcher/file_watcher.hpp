#pragma once

#include "lumina_types.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace lumina::common {

// Polls a set of files for modification and reports the ones that change.
//
// Work is spread rather than batched: each slice examines a few files from a
// round-robin sweep, so the cost per frame stays flat instead of spiking every
// time the whole set is checked at once. A file seen to have changed moves into
// a candidate list that is re-examined every slice, so a change is confirmed
// promptly without waiting for the sweep to come back around to it.
//
// A change is only handed to its callback once the file has held still for
// settle_ms. Writers are not atomic in general, so a file caught mid-write
// would otherwise be reported while it is still being written; requiring it to
// stop moving first is what makes that unlikely. It is not a guarantee, and is
// not meant to be one.
class FileWatcher {
public:
  // The callback returns whether it consumed the change. Returning false — a
  // failed read, a file that did not pass the consumer's own format check —
  // leaves the change pending, so the same version is offered again on a later
  // slice rather than being lost.
  using OnFileChanged = std::function<bool(const std::string &)>;

  struct Config {
    // Wall time between slices, and so the rate at which candidates are
    // re-examined.
    u32 slice_interval_ms = 50;

    // Files examined per slice by the sweep. Holds the per-frame cost flat, at
    // the price of a sweep that takes
    // (file count / files_per_slice) * slice_interval_ms to come back around —
    // which is how long a change can go unnoticed before it is spotted.
    u32 files_per_slice = 1;

    // How long a changed file must hold still before it is handed over. Kept
    // as a duration rather than a number of polls because a file's polling
    // cadence depends on whether it is in the sweep or the candidate list.
    u32 settle_ms = 150;
  };

  FileWatcher() noexcept = default;
  explicit FileWatcher(Config p_config) noexcept;
  FileWatcher(const FileWatcher &) noexcept = delete;
  FileWatcher(FileWatcher &&) noexcept = default;
  auto operator=(const FileWatcher &) noexcept -> FileWatcher & = delete;
  auto operator=(FileWatcher &&) noexcept -> FileWatcher & = default;
  ~FileWatcher() noexcept = default;

  // Starts watching a file. Its current state is taken as already seen, so an
  // unchanged file is not reported on the first sweep.
  auto Watch(std::string p_path, OnFileChanged p_on_file_changed) -> void;

  // Advances one slice, if slice_interval_ms has passed since the last one.
  // Cheap to call every frame; that is what it is for.
  auto PollAll() -> void;

  [[nodiscard]] auto WatchedCount() const noexcept -> std::size_t {
    return watched.size();
  }

private:
  struct WatchedFile {
    std::string path;
    OnFileChanged on_file_changed = nullptr;

    // The version last handed over and accepted. Held as plain fields rather
    // than a platform::common::FileMetadata so this header stays clear of the
    // platform layer, which only the .cpp needs.
    u64 consumed_write_time = 0;
    u64 consumed_size = 0;

    // A change seen but not yet handed over, and when it was first seen in its
    // current form. has_candidate distinguishes "nothing pending" from a
    // candidate whose fields happen to be zero.
    u64 candidate_write_time = 0;
    u64 candidate_size = 0;
    u64 candidate_seen_at_ms = 0;
    bool has_candidate = false;
  };

  // Examines one file and returns whether it has a change still pending
  // afterwards, which is what decides its place in the candidate list. The one
  // place a callback is invoked.
  auto ExamineFile(WatchedFile &file, u64 now_ms) -> bool;

  std::vector<WatchedFile> watched;

  // Indices into watched, not pointers, so growing the vector cannot
  // invalidate them.
  std::vector<u32> candidates;

  Config config{};
  u64 last_slice_time = 0;
  u32 sweep_index = 0;
};

} // namespace lumina::common
