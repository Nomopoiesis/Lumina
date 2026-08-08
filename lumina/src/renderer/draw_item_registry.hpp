#pragma once

#include "common/lumina_check.hpp"
#include "graphics_pipeline_handle.hpp"
#include "material_instance_handle.hpp"
#include "render_mesh.hpp"

#include <unordered_map>
#include <vector>

namespace lumina::renderer {

struct DrawItem {
  GraphicsPipelineHandle pipeline_handle;
  MaterialInstanceHandle material_instance_handle;
  RenderMeshHandle render_mesh_handle;
};

class DrawItemRegistry {
public:
  DrawItemRegistry() = default;
  DrawItemRegistry(const DrawItemRegistry &) = delete;
  DrawItemRegistry(DrawItemRegistry &&) noexcept = delete;
  auto operator=(const DrawItemRegistry &) -> DrawItemRegistry & = delete;
  auto operator=(DrawItemRegistry &&) noexcept -> DrawItemRegistry & = delete;
  ~DrawItemRegistry() = default;

  auto AcquireDrawItem(GraphicsPipelineHandle pipeline_handle,
                       MaterialInstanceHandle material_instance_handle,
                       RenderMeshHandle render_mesh_handle) -> u32;

  [[nodiscard]] auto Get(u32 item_index) const -> const DrawItem & {
    LUMINA_CHECK(item_index < draw_items.size(), "Invalid draw item index");
    return draw_items[item_index];
  }

  [[nodiscard]] auto GetCount() const -> size_t { return draw_items.size(); }

private:
  std::unordered_map<u64, u32> lookup_table;
  std::vector<DrawItem> draw_items;
};

} // namespace lumina::renderer