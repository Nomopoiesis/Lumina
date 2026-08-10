#include "draw_item_registry.hpp"

#include "common/lumina_util.hpp"

namespace lumina::renderer {

auto DrawItemRegistry::AcquireDrawItem(
    GraphicsPipelineHandle pipeline_handle,
    MaterialInstanceHandle material_instance_handle,
    RenderMeshHandle render_mesh_handle) -> u32 {
  // combined render mesh and material instance index into a single key
  auto key = (static_cast<u64>(render_mesh_handle.index) << 32) |
             (static_cast<u64>(material_instance_handle.index));
  auto it = lookup_table.find(key);
  if (it != lookup_table.end()) {
    LUMINA_CHECK(draw_items[it->second].pipeline_handle == pipeline_handle,
                 "Draw item pipeline mismatch: same (material, mesh) acquired "
                 "with two different pipelines");
    return it->second;
  }
  auto draw_item =
      DrawItem{.pipeline_handle = pipeline_handle,
               .material_instance_handle = material_instance_handle,
               .render_mesh_handle = render_mesh_handle};
  const auto item_index = SafeU64ToU32(draw_items.size());
  lookup_table[key] = item_index;
  draw_items.push_back(draw_item);
  return item_index;
}

} // namespace lumina::renderer
