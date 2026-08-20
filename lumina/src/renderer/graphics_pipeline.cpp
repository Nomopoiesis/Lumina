#include "graphics_pipeline.hpp"

#include "shaders/shader_module_cache.hpp"
#include "vertex_layout.hpp"
#include "vk_helpers.hpp"

namespace {

using namespace lumina;
using namespace lumina::renderer;

auto ToVkBindingDescriptions(const VertexBufferLayout &layout)
    -> std::vector<VkVertexInputBindingDescription> {
  std::vector<VkVertexInputBindingDescription> descriptions;
  descriptions.reserve(layout.streams.size());
  for (u32 binding = 0; binding < static_cast<u32>(layout.streams.size());
       ++binding) {
    u32 stride = 0;
    for (const auto &attr : layout.streams[binding].attributes) {
      stride += core::GetElementTypeSize(attr.element_type);
    }
    descriptions.push_back({
        .binding = binding,
        .stride = stride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    });
  }
  return descriptions;
}

auto ToVkAttributeDescriptions(const VertexBufferLayout &layout,
                               const VertexInputLayout &vertex_input_layout)
    -> std::expected<std::vector<VkVertexInputAttributeDescription>,
                     common::LuminaError> {
  std::vector<VkVertexInputAttributeDescription> descriptions;
  for (u32 binding = 0; binding < static_cast<u32>(layout.streams.size());
       ++binding) {
    u32 offset = 0;
    for (const auto &attr : layout.streams[binding].attributes) {
      u32 location = UINT32_MAX;
      for (u32 i = 0; i < vertex_input_layout.input_count; ++i) {
        if (vertex_input_layout.inputs[i].attribute_type == attr.type) {
          location = vertex_input_layout.inputs[i].location;
          if (vertex_input_layout.inputs[i].element_type != attr.element_type) {
            return std::unexpected(
                common::LuminaError("Vertex attribute element type does not "
                                    "match shader vertex input layout"));
          }
          break;
        }
      }
      // An attribute the shader does not read is legal and expected: the pick
      // pass takes position out of the full scene vertex layout and ignores
      // normal and texcoord, and a depth or shadow pass would do the same.
      // Only the offset has to keep advancing regardless, or every attribute
      // after the skipped one lands at the wrong place in the stride.
      if (location != UINT32_MAX) {
        descriptions.push_back({
            .location = location,
            .binding = binding,
            .format = ToVkFormat(attr.element_type),
            .offset = offset,
        });
      }
      offset += core::GetElementTypeSize(attr.element_type);
    }
  }

  // The direction that actually matters, and that nothing checked before: a
  // shader input with no attribute behind it reads undefined vertex data
  // instead of failing. The reverse — spare attributes — is what the loop above
  // now tolerates.
  if (descriptions.size() != vertex_input_layout.input_count) {
    return std::unexpected(common::LuminaError(
        "Shader declares a vertex input that the vertex buffer layout "
        "does not supply"));
  }
  return descriptions;
}

} // namespace

namespace lumina::renderer {

auto CreateGraphicsPipeline(const VkDevice &device,
                            const GraphicsPipelineShaderInputs &inputs,
                            const GraphicsPipelineDesc &desc)
    -> std::expected<GraphicsPipeline, common::LuminaError> {

  if (desc.enable_depth_test &&
      desc.depth_stencil_attachment_format == VK_FORMAT_UNDEFINED) {
    return std::unexpected(common::LuminaError(
        "CreateGraphicsPipeline: Depth test enabled but no depth format "
        "specified"));
  }

  ShaderModuleCache shader_module_cache(device);
  auto vert_module_result = shader_module_cache.GetShaderModule(
      inputs.vertex_shader_bin_path, VK_SHADER_STAGE_VERTEX_BIT);
  if (!vert_module_result) {
    return std::unexpected(common::LuminaError(
        "CreateGraphicsPipeline: Failed to create vertex shader"));
  }

  auto frag_module_result = shader_module_cache.GetShaderModule(
      inputs.fragment_shader_bin_path, VK_SHADER_STAGE_FRAGMENT_BIT);
  if (!frag_module_result) {
    return std::unexpected(common::LuminaError(
        "CreateGraphicsPipeline: Failed to create fragment shader"));
  }

  VkPipelineShaderStageCreateInfo shader_stages[] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vert_module_result.value(),
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = frag_module_result.value(),
          .pName = "main",
      },
  };

  std::vector<VkDynamicState> dynamic_states = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data(),
  };

  auto binding_descriptions = ToVkBindingDescriptions(desc.vertex_layout);
  auto attribute_descriptions_result =
      ToVkAttributeDescriptions(desc.vertex_layout, inputs.vertex_input_layout);
  if (!attribute_descriptions_result) {
    auto message =
        "CreateGraphicsPipeline: Failed to create attribute descriptions -> " +
        std::string(attribute_descriptions_result.error().Message());
    return std::unexpected(common::LuminaError(message));
  }
  auto attribute_descriptions = attribute_descriptions_result.value();

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount =
          static_cast<u32>(binding_descriptions.size()),
      .pVertexBindingDescriptions = binding_descriptions.data(),
      .vertexAttributeDescriptionCount =
          static_cast<u32>(attribute_descriptions.size()),
      .pVertexAttributeDescriptions = attribute_descriptions.data(),
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .topology = ToVkPrimitiveTopology(desc.topology),
      .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineViewportStateCreateInfo viewport_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };

  VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = desc.cull_mode,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0F,
      .depthBiasClamp = 0.0F,
      .depthBiasSlopeFactor = 0.0F,
      .lineWidth = 1.0F,
  };

  VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 0.0F,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
  color_blend_attachment_state.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  if (desc.enable_blending) {
    // Standard alpha blending: out = src.rgb * src.a + dst.rgb * (1 - src.a)
    color_blend_attachment_state.blendEnable = VK_TRUE;
    color_blend_attachment_state.srcColorBlendFactor =
        VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
  } else {
    color_blend_attachment_state.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment_state,
      .blendConstants = {0.0F, 0.0F, 0.0F, 0.0F},
  };

  VkPipelineRenderingCreateInfoKHR pipeline_dynamic_rendering_info{};
  pipeline_dynamic_rendering_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  pipeline_dynamic_rendering_info.pNext = nullptr;
  pipeline_dynamic_rendering_info.colorAttachmentCount =
      static_cast<u32>(desc.color_attachment_formats.size());
  pipeline_dynamic_rendering_info.pColorAttachmentFormats =
      desc.color_attachment_formats.data();
  pipeline_dynamic_rendering_info.depthAttachmentFormat =
      desc.depth_stencil_attachment_format;
  pipeline_dynamic_rendering_info.stencilAttachmentFormat =
      HasStencilComponent(desc.depth_stencil_attachment_format)
          ? desc.depth_stencil_attachment_format
          : VK_FORMAT_UNDEFINED;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{};
  depth_stencil_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state_create_info.depthTestEnable =
      desc.enable_depth_test ? VK_TRUE : VK_FALSE;
  depth_stencil_state_create_info.depthWriteEnable =
      desc.enable_depth_test ? VK_TRUE : VK_FALSE;
  depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
  depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
  depth_stencil_state_create_info.front = {};
  depth_stencil_state_create_info.back = {};
  depth_stencil_state_create_info.minDepthBounds = 0.0F;
  depth_stencil_state_create_info.maxDepthBounds = 1.0F;

  VkGraphicsPipelineCreateInfo pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &pipeline_dynamic_rendering_info,
      .flags = 0,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_state_create_info,
      .pInputAssemblyState = &input_assembly_state_create_info,
      .pViewportState = &viewport_state_create_info,
      .pRasterizationState = &rasterization_state_create_info,
      .pMultisampleState = &multisample_state_create_info,
      .pDepthStencilState = &depth_stencil_state_create_info,
      .pColorBlendState = &color_blend_state_create_info,
      .pDynamicState = &dynamic_state_create_info,
      .layout = inputs.pipeline_layout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  GraphicsPipeline gfx_pipeline;
  if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_create_info,
                                nullptr,
                                &gfx_pipeline.vk_pipeline) != VK_SUCCESS) {
    return std::unexpected(common::LuminaError(
        "CreateGraphicsPipeline: Failed to create graphics pipeline"));
  }
  gfx_pipeline.desc = desc;
  return gfx_pipeline;
}

} // namespace lumina::renderer
