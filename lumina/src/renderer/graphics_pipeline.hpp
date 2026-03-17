#pragma once

#include "material_template_handle.hpp"

#include "vertex_layout.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

struct GraphicsPipelineDesc {
  VertexBufferLayout vertex_layout;
  MaterialTemplateHandle material_template;
};

struct GraphicsPipeline {
  VkPipeline vk_pipeline = VK_NULL_HANDLE;
  VertexBufferLayout vertex_layout;
  MaterialTemplateHandle material_template;
};

} // namespace lumina::renderer
