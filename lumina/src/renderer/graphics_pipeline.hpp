#pragma once

#include "common/lumina_error.hpp"
#include "material_template_handle.hpp"
#include "primitive_topology.hpp"
#include "shaders/shader_layout.hpp"
#include "vertex_layout.hpp"

#include <vulkan/vulkan.h>

#include <expected>
#include <string>

namespace lumina::renderer {

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

  u64 reload_epoch = 0;
};

struct GraphicsPipelineShaderInputs {
  std::string vertex_shader_bin_path;
  std::string fragment_shader_bin_path;
  VertexInputLayout vertex_input_layout;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
};

auto CreateGraphicsPipeline(const VkDevice &device,
                            const GraphicsPipelineShaderInputs &inputs,
                            const GraphicsPipelineDesc &desc)
    -> std::expected<GraphicsPipeline, common::LuminaError>;

} // namespace lumina::renderer
