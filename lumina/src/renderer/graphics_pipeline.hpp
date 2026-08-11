#pragma once

#include "common/lumina_error.hpp"
#include "material_template_handle.hpp"
#include "primitive_topology.hpp"
#include "vertex_layout.hpp"

#include <vulkan/vulkan.h>

#include <expected>

namespace lumina::renderer {

class VulkanContext;
class MaterialTemplate;
class ShaderInterface;

struct GraphicsPipelineDesc {
  VertexBufferLayout vertex_layout;
  MaterialTemplateHandle material_template;
  PrimitiveTopology topology = PrimitiveTopology::TriangleList;
  std::vector<VkFormat> color_attachment_formats;
  VkFormat depth_stencil_attachment_format = VK_FORMAT_UNDEFINED;
};

struct GraphicsPipeline {
  VkPipeline vk_pipeline = VK_NULL_HANDLE;
  GraphicsPipelineDesc desc;
};

auto CreateGraphicsPipeline(VulkanContext &vulkan_context,
                            const MaterialTemplate &material_template,
                            const ShaderInterface &shader_interface,
                            const GraphicsPipelineDesc &desc)
    -> std::expected<GraphicsPipeline, common::LuminaError>;

} // namespace lumina::renderer
