#include "drawable_proxy_manager.hpp"

#include "components/static_mesh_component.hpp"

namespace lumina::core {

auto DrawableProxyManager::Sync(
    World &world, StaticMeshResourceRegistry &static_mesh_registry,
    renderer::LuminaRenderer &renderer) -> void {
  // TODO: first version of render proxy scene, we rebuild the entire scene
  // every frame, later we will optimize it to only rebuild the changed parts.
  center_x.clear();
  center_y.clear();
  center_z.clear();
  extent_x.clear();
  extent_y.clear();
  extent_z.clear();
  model.clear();
  draw_item_indices.clear();

  world.ForEachComponent<components::StaticMeshComponent>(
      [this, &world, &static_mesh_registry,
       &renderer](EntityID id,
                  const components::StaticMeshComponent &component) -> void {
        auto static_mesh_handle = component.GetStaticMeshHandle();
        auto static_mesh_opt = static_mesh_registry.Get(static_mesh_handle);
        if (!static_mesh_opt.has_value()) {
          return;
        }
        auto &static_mesh = static_mesh_opt.value();

        // The GPU upload is deferred, so a mesh can exist here several frames
        // before it has a render mesh. Emitting a proxy for it would put an
        // invalid handle straight into the draw list.
        if (static_mesh->render_mesh_handle.index ==
            INVALID_RESOURCE_HANDLE_INDEX) {
          return;
        }

        auto render_mesh_opt =
            renderer.GetRenderMesh(static_mesh->render_mesh_handle);
        if (!render_mesh_opt.has_value()) {
          return;
        }
        auto &render_mesh = render_mesh_opt.value();

        // The mesh AABB is in model space; the frustum planes are in world
        // space, so it has to be transformed here or every proxy would be
        // tested as if it sat at the origin. TransformToCenterExtent is the
        // abs-matrix form — ~18 flops instead of pushing all 8 corners through
        // the matrix.
        const auto model_matrix = world.GetTransform(id).GetModelMatrix();
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
        material_instance =
            (material_instance.index == INVALID_RESOURCE_HANDLE_INDEX
                 ? renderer.GetDefaultMaterialInstanceHandle()
                 : material_instance);

        auto draw_item_index = renderer.GetDrawItemRegistry().AcquireDrawItem(
            render_mesh->pipeline_handle, material_instance,
            static_mesh->render_mesh_handle);
        draw_item_indices.push_back(draw_item_index);
      });
}

} // namespace lumina::core
