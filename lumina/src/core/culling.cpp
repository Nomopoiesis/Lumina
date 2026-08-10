#include "culling.hpp"

#include "common/profiling/profiler.hpp"
#include "job_system/job_manager.hpp"
#include "math/matrix.hpp"

#include <algorithm>

namespace lumina::core {

namespace {

// Culls [begin, end) into the slice starting at `begin` and returns how many
// survived. The slice is exactly the range handed in, so chunks never overlap
// and need no synchronisation.
[[nodiscard]] auto CullChunk(const DrawableProxyManager &proxies,
                             const Frustum &frustum, size_t begin, size_t end,
                             BatchedVisibilityIndex &visibility) -> size_t {
  size_t written = 0;
  for (size_t i = begin; i < end; ++i) {
    if (TestAABoundingBox(frustum, proxies.GetProxyAABB(i)) ==
        FrustumTestResult::Outside) {
      continue;
    }
    visibility.SetVisibleIndex(begin + written, i);
    ++written;
  }
  return written;
}

} // namespace

auto FrustumCulling(const DrawableProxyManager &proxies, const Frustum &frustum,
                    BatchedVisibilityIndex &visibility) -> void {
  // Wall time, so this includes the fiber sitting parked in WaitForCounter
  // while workers chew through chunks. That is "how long the cull took", not
  // "how much CPU the cull burned" — the latter needs the per-thread breakdown.
  LUMINA_PROFILE_SCOPE("Cull");

  const auto count = proxies.ProxyCount();
  visibility.Reset(count, CullBatchSize);

  job_system::ParallelForBatched(
      count, CullBatchSize, [&](size_t begin, size_t end) -> void {
        const size_t written =
            CullChunk(proxies, frustum, begin, end, visibility);
        visibility.SetChunkSize(begin / CullBatchSize, written);
      });
}

#if 0 // NOLINT
auto FrustumCullingSerial(const DrawableProxyManager &proxies,
                      const Frustum &frustum,
                      BatchedVisibilityIndex &visibility) -> void {
  const auto count = proxies.ProxyCount();
  visibility.Reset(count, CullBatchSize);

  // Chunked rather than one flat loop so the output matches FrustumCulling exactly.
  for (size_t begin = 0; begin < count; begin += CullBatchSize) {
  const size_t end = std::min(begin + CullBatchSize, count);
  const size_t written = CullChunk(proxies, frustum, begin, end, visibility);
  visibility.SetChunkSize(begin / CullBatchSize, written);
  }
}
#endif

} // namespace lumina::core
