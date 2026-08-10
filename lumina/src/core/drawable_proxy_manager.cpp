#include "drawable_proxy_manager.hpp"

#include "common/logger/logger.hpp"
#include "common/profiling/profiler.hpp"
#include "components/static_mesh_component.hpp"

namespace lumina::core {

auto DrawableProxyManager::ProcessProxy(
    World &world, StaticMeshResourceRegistry &static_mesh_registry,
    renderer::LuminaRenderer &renderer, EntityID entity_id) -> bool {
  const auto &component =
      world.GetComponent<components::StaticMeshComponent>(entity_id);
  auto static_mesh_handle = component.GetStaticMeshHandle();
  auto static_mesh_opt = static_mesh_registry.Get(static_mesh_handle);
  if (!static_mesh_opt.has_value()) {
    return false;
  }
  auto &static_mesh = static_mesh_opt.value();

  // The GPU upload is deferred, so a mesh can exist here several frames
  // before it has a render mesh. Emitting a proxy for it would put an
  // invalid handle straight into the draw list.
  if (static_mesh->render_mesh_handle.index == INVALID_RESOURCE_HANDLE_INDEX) {
    return false;
  }

  auto render_mesh_opt =
      renderer.GetRenderMesh(static_mesh->render_mesh_handle);
  if (!render_mesh_opt.has_value()) {
    return false;
  }
  auto &render_mesh = render_mesh_opt.value();

  // The mesh AABB is in model space; the frustum planes are in world
  // space, so it has to be transformed here or every proxy would be
  // tested as if it sat at the origin. TransformToCenterExtent is the
  // abs-matrix form — ~18 flops instead of pushing all 8 corners through
  // the matrix.
  const auto model_matrix = world.GetTransform(entity_id).GetModelMatrix();
  const auto bounds =
      TransformToCenterExtent(static_mesh->bounding_box, model_matrix);

  center_x.push_back(bounds.center.x);
  center_y.push_back(bounds.center.y);
  center_z.push_back(bounds.center.z);
  extent_x.push_back(bounds.extent.x);
  extent_y.push_back(bounds.extent.y);
  extent_z.push_back(bounds.extent.z);

  model.push_back(model_matrix);

  // Entities created without a material fall back to the renderer's
  // default, so nothing that only sets a mesh has to change.
  auto material_instance = component.GetMaterialInstanceHandle();
  material_instance = (material_instance.index == INVALID_RESOURCE_HANDLE_INDEX
                           ? renderer.GetDefaultMaterialInstanceHandle()
                           : material_instance);

  auto draw_item_index = renderer.GetDrawItemRegistry().AcquireDrawItem(
      render_mesh->pipeline_handle, material_instance,
      static_mesh->render_mesh_handle);
  draw_item_indices.push_back(draw_item_index);

  entity_ids.push_back(entity_id);

  return true;
}

auto DrawableProxyManager::Sync(
    World &world, StaticMeshResourceRegistry &static_mesh_registry,
    renderer::LuminaRenderer &renderer) -> void {
  LUMINA_PROFILE_SCOPE("Sync");

  auto added_static_mesh_components =
      world.GetAddedComponents<components::StaticMeshComponent>();

  for (auto entity_id : added_static_mesh_components) {
    if (world.GetEntity(entity_id)->GetMobility() == Mobility::Static) {
      pending_static_entities.push_back(entity_id);
    } else {
      dynamic_entities.push_back(entity_id);
    }
  }

  // Resize the arrays to the static proxy count - effectively clearing dynamic
  // proxies which we will rebuild later.
  center_x.resize(static_proxy_count);
  center_y.resize(static_proxy_count);
  center_z.resize(static_proxy_count);
  extent_x.resize(static_proxy_count);
  extent_y.resize(static_proxy_count);
  extent_z.resize(static_proxy_count);
  model.resize(static_proxy_count);
  draw_item_indices.resize(static_proxy_count);
  entity_ids.resize(static_proxy_count);

  // Process pending static entities that were added since the last frame.
  ProcessPendingStaticEntities(world, static_mesh_registry, renderer);

  // Process registered dynamic entities
  ProcessRegisteredDynamicEntities(world, static_mesh_registry, renderer);
}

auto DrawableProxyManager::ProcessPendingStaticEntities(
    World &world, StaticMeshResourceRegistry &static_mesh_registry,
    renderer::LuminaRenderer &renderer) -> void {
  std::vector<EntityID> remaining_entities;
  for (auto entity_id : pending_static_entities) {
    if (ProcessProxy(world, static_mesh_registry, renderer, entity_id)) {
      ++static_proxy_count;
    } else {
      remaining_entities.push_back(entity_id);
    }
  }
  pending_static_entities = remaining_entities;
}

auto DrawableProxyManager::ProcessRegisteredDynamicEntities(
    World &world, StaticMeshResourceRegistry &static_mesh_registry,
    renderer::LuminaRenderer &renderer) -> void {
  for (auto entity_id : dynamic_entities) {
    if (!ProcessProxy(world, static_mesh_registry, renderer, entity_id)) {
      LOG_WARNING("Failed to process dynamic entity: {}", entity_id);
    }
  }
}
} // namespace lumina::core
