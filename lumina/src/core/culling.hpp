#pragma once

#include "drawable_proxy_manager.hpp"
#include "frustum.hpp"
#include "renderer/frame_context.hpp"

#include <vector>

namespace lumina::core {

// Chunk width for the parallel cull. This also sets the job count, since
// ParallelForBatched submits one job per chunk.
inline constexpr size_t CullBatchSize = 1024;

// Records which proxies survive the frustum into `visibility`, which is sized
// to fit and is meant to be reused across frames.
auto CullProxies(const DrawableProxyManager &proxies, const Frustum &frustum,
                 BatchedVisibilityIndex &visibility) -> void;

#if 0 // NOLINT
// Single-threaded reference implementation, kept so the two can be A/B timed.
// It fills `visibility` identically, chunk for chunk, so everything downstream
// — and any comparison between them — is unaffected by which one ran.
auto CullProxiesSerial(const DrawableProxyManager &proxies,
    const Frustum &frustum,
    BatchedVisibilityIndex &visibility) -> void;
#endif
} // namespace lumina::core
