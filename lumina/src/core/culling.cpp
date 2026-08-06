#include "culling.hpp"

#include "job_system/job_manager.hpp"
#include "math/matrix.hpp"

#include <algorithm>

namespace lumina::core {

namespace {

[[nodiscard]] auto ProxyBounds(const DrawableProxyManager &proxies,
                               size_t index) -> AABoundingBoxCenterExtent {
  return {
      .center = math::Vec3{proxies.center_x[index], proxies.center_y[index],
                           proxies.center_z[index]},
      .extent = math::Vec3{proxies.extent_x[index], proxies.extent_y[index],
                           proxies.extent_z[index]},
  };
}

// Culls [begin, end) into the slice starting at `begin` and returns how many
// survived. The slice is exactly the range handed in, so chunks never overlap
// and need no synchronisation.
[[nodiscard]] auto CullChunk(const DrawableProxyManager &proxies,
                             const Frustum &frustum, size_t begin, size_t end,
                             BatchedVisibilityIndex &visibility) -> size_t {
  size_t written = 0;
  for (size_t i = begin; i < end; ++i) {
    if (TestAABoundingBox(frustum, ProxyBounds(proxies, i)) ==
        FrustumTestResult::Outside) {
      continue;
    }
    visibility.SetVisibleIndex(begin + written, i);
    ++written;
  }
  return written;
}

template <typename F>
auto ForEachVisible(const BatchedVisibilityIndex &visibility, F &&emit)
    -> void {
  for (size_t chunk = 0; chunk < visibility.GetChunkCount(); ++chunk) {
    const size_t base = chunk * CullBatchSize;
    const size_t visible = visibility.GetChunkSize(chunk);
    for (size_t n = 0; n < visible; ++n) {
      emit(visibility.GetVisibleIndex(base + n));
    }
  }
}

} // namespace

auto CullProxies(const DrawableProxyManager &proxies, const Frustum &frustum,
                 BatchedVisibilityIndex &visibility) -> void {
  const auto count = proxies.ProxyCount();
  visibility.Reset(count, CullBatchSize);

  job_system::ParallelForBatched(
      count, CullBatchSize, [&](size_t begin, size_t end) -> void {
        const size_t written =
            CullChunk(proxies, frustum, begin, end, visibility);
        visibility.SetChunkSize(begin / CullBatchSize, written);
      });
}

auto AppendDrawCommands(const DrawableProxyManager &proxies,
                        const BatchedVisibilityIndex &visibility,
                        std::vector<renderer::DrawCommand> &draw_list) -> void {
  ForEachVisible(visibility, [&](size_t index) -> void {
    draw_list.emplace_back(renderer::DrawMeshCommand{
        .render_mesh_handle = proxies.mesh_handle[index],
        .material_instance = proxies.material[index],
        .model = proxies.model[index]});
  });
}

auto AppendDebugAABBDrawCommands(const DrawableProxyManager &proxies,
                                 const BatchedVisibilityIndex &visibility,
                                 renderer::RenderMeshHandle debug_mesh,
                                 std::vector<renderer::DrawCommand> &draw_list)
    -> void {
  ForEachVisible(visibility, [&](size_t index) -> void {
    const auto bounds = ProxyBounds(proxies, index);
    draw_list.emplace_back(renderer::DrawDebugAABBCommand{
        .render_mesh_handle = debug_mesh,
        .model = math::Dot(math::ScaleMatrix(bounds.extent * 2.0F),
                           math::TranslationMatrix(bounds.center))});
  });
}

#if 0 // NOLINT
auto CullProxiesSerial(const DrawableProxyManager &proxies,
                      const Frustum &frustum,
                      BatchedVisibilityIndex &visibility) -> void {
  const auto count = proxies.ProxyCount();
  visibility.Reset(count, CullBatchSize);

  // Chunked rather than one flat loop so the output matches CullProxies exactly.
  for (size_t begin = 0; begin < count; begin += CullBatchSize) {
  const size_t end = std::min(begin + CullBatchSize, count);
  const size_t written = CullChunk(proxies, frustum, begin, end, visibility);
  visibility.SetChunkSize(begin / CullBatchSize, written);
  }
}
#endif

} // namespace lumina::core
