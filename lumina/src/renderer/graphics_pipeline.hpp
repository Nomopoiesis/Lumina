#pragma once

#include "render_mesh.hpp"
#include "vertex_layout.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

struct GraphicsPipelineDesc {
  VertexBufferLayout vertex_layout;
};

struct GraphicsPipeline {
  VkPipeline vk_pipeline = VK_NULL_HANDLE;
  VertexBufferLayout vertex_layout;
};

using GraphicsPipelineManager = core::ResourceManager<GraphicsPipeline>;

} // namespace lumina::renderer
