#pragma once

#include "material_template_handle.hpp"
#include "primitive_topology.hpp"
#include "vertex_layout.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

struct GraphicsPipelineDesc {
  VertexBufferLayout vertex_layout;
  MaterialTemplateHandle material_template;
  PrimitiveTopology topology = PrimitiveTopology::TriangleList;
};

struct GraphicsPipeline {
  VkPipeline vk_pipeline = VK_NULL_HANDLE;
  VertexBufferLayout vertex_layout;
  MaterialTemplateHandle material_template;
  PrimitiveTopology topology = PrimitiveTopology::TriangleList;
};

} // namespace lumina::renderer
