#include "drawable_proxy_scene.hpp"

#include "core/components/static_mesh_component.hpp"

namespace lumina::renderer {

auto DrawableProxyScene::Sync(core::World &world,
                              core::StaticMeshManager &static_mesh_manager,
                              LuminaRenderer &renderer) -> void {
  // TODO: first version of render proxy scene, we rebuild the entire scene
  // every frame, later we will optimize it to only rebuild the changed parts.
  center_x.clear();
  center_y.clear();
  center_z.clear();
  extent_x.clear();
  extent_y.clear();
  extent_z.clear();
  model.clear();
  mesh_handle.clear();
  material.clear();
  world.ForEachComponent<core::components::StaticMeshComponent>(
      [this, &world, &static_mesh_manager, &renderer](
          core::EntityID id,
          const core::components::StaticMeshComponent &component) -> void {
        auto static_mesh_handle = component.GetStaticMeshHandle();
        auto static_mesh_opt = static_mesh_manager.Get(static_mesh_handle);
        if (!static_mesh_opt.has_value()) {
          return;
        }
        auto &static_mesh = static_mesh_opt.value();
        const auto &bounding_box = static_mesh->bounding_box;
        auto aabbce = core::ToCenterExtent(bounding_box);
        center_x.push_back(aabbce.center.x);
        center_y.push_back(aabbce.center.y);
        center_z.push_back(aabbce.center.z);
        extent_x.push_back(aabbce.extent.x);
        extent_y.push_back(aabbce.extent.y);
        extent_z.push_back(aabbce.extent.z);
        model.push_back(world.GetTransform(id).GetModelMatrix());
        mesh_handle.push_back(static_mesh->render_mesh_handle);
        material.push_back(renderer.GetDefaultMaterialInstanceHandle());
      });
}

} // namespace lumina::renderer