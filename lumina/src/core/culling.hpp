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

// Appends one DrawMeshCommand per visible proxy, walking chunks in index order
// so the resulting draw list is deterministic.
auto AppendDrawCommands(const DrawableProxyManager &proxies,
                        const BatchedVisibilityIndex &visibility,
                        std::vector<renderer::DrawCommand> &draw_list) -> void;

// Appends one DrawDebugAABBCommand per visible proxy, using `debug_mesh` as the
// wireframe box.
//
// Deliberately a separate pass rather than a flag threaded through the cull:
// visibility and debug visualization are unrelated concerns, and the renderer
// binds a pipeline per draw command, so emitting all the mesh commands before
// all the debug ones stops it alternating pipelines on every single draw.
auto AppendDebugAABBDrawCommands(const DrawableProxyManager &proxies,
                                 const BatchedVisibilityIndex &visibility,
                                 renderer::RenderMeshHandle debug_mesh,
                                 std::vector<renderer::DrawCommand> &draw_list)
    -> void;

#if 0 // NOLINT
// Single-threaded reference implementation, kept so the two can be A/B timed.
// It fills `visibility` identically, chunk for chunk, so everything downstream
// — and any comparison between them — is unaffected by which one ran.
auto CullProxiesSerial(const DrawableProxyManager &proxies,
    const Frustum &frustum,
    BatchedVisibilityIndex &visibility) -> void;
#endif
} // namespace lumina::core
