#include "ui_renderer.hpp"

#include "common/logger/logger.hpp"
#include "common/lumina_assert.hpp"
#include "common/path_registry.hpp"
#include "platform/common/platform_services.hpp"

#include <cstring>
#include <vector>

namespace lumina::renderer {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static auto LoadSpirV(const std::string &path) -> std::vector<u8> {
  void *handle = platform::common::PlatformServices::Instance().LuminaOpenFile(
      path.c_str());
  ASSERT(handle != nullptr, "Failed to open UI shader SPIR-V");
  const size_t size =
      platform::common::PlatformServices::Instance().LuminaGetFileSize(handle);
  ASSERT(size > 0, "UI shader SPIR-V is empty");
  std::vector<u8> data(size);
  platform::common::PlatformServices::Instance().LuminaReadFile(
      handle, data.data(), size);
  platform::common::PlatformServices::Instance().LuminaCloseFile(handle);
  return data;
}

static auto FindMemoryTypeIndex(VulkanContext &ctx, u32 type_bits,
                                VkMemoryPropertyFlags props) -> u32 {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(ctx.GetPhysicalDevice(), &mem_props);
  for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
    if ((type_bits & (1u << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }
  ASSERT(false, "No suitable memory type for UI buffer");
  return 0;
}

static auto CreateHostBuffer(VulkanContext &ctx, VkDeviceSize size,
                             VkBufferUsageFlags usage, VkBuffer &buffer,
                             VkDeviceMemory &memory, void *&mapped) -> void {
  VkBufferCreateInfo buf_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  ASSERT(vkCreateBuffer(ctx.GetDevice(), &buf_info, nullptr, &buffer) ==
             VK_SUCCESS,
         "Failed to create UI buffer");

  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(ctx.GetDevice(), buffer, &req);

  VkMemoryAllocateInfo alloc{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = req.size,
      .memoryTypeIndex =
          FindMemoryTypeIndex(ctx, req.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };
  ASSERT(vkAllocateMemory(ctx.GetDevice(), &alloc, nullptr, &memory) ==
             VK_SUCCESS,
         "Failed to allocate UI buffer memory");
  vkBindBufferMemory(ctx.GetDevice(), buffer, memory, 0);
  vkMapMemory(ctx.GetDevice(), memory, 0, size, 0, &mapped);
}

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

auto UIRenderer::Initialize(VulkanContext &ctx,
                            const std::string &vert_spv_path,
                            const std::string &frag_spv_path,
                            VkFormat depth_format) -> void {
  CreateDescriptors(ctx);
  CreatePipeline(ctx, vert_spv_path, frag_spv_path, depth_format);
  CreateBuffers(ctx);
}

auto UIRenderer::Shutdown(VulkanContext &ctx) -> void {
  auto *device = ctx.GetDevice();
  for (auto &fr : frame_resources) {
    if (fr.vertex_buffer != VK_NULL_HANDLE) {
      vkUnmapMemory(device, fr.vertex_buffer_memory);
      vkDestroyBuffer(device, fr.vertex_buffer, nullptr);
      vkFreeMemory(device, fr.vertex_buffer_memory, nullptr);
    }
    if (fr.index_buffer != VK_NULL_HANDLE) {
      vkUnmapMemory(device, fr.index_buffer_memory);
      vkDestroyBuffer(device, fr.index_buffer, nullptr);
      vkFreeMemory(device, fr.index_buffer_memory, nullptr);
    }
  }
  if (descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
  }
  if (pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, pipeline, nullptr);
  }
  if (pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  }
  if (descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
  }
}

// ---------------------------------------------------------------------------
// SetFontAtlas
// ---------------------------------------------------------------------------

auto UIRenderer::SetFontAtlas(VulkanContext &ctx, VkImageView image_view,
                              VkSampler sampler) -> void {
  for (auto &fr : frame_resources) {
    VkDescriptorImageInfo img{
        .sampler = sampler,
        .imageView = image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = fr.descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &img,
    };
    vkUpdateDescriptorSets(ctx.GetDevice(), 1, &write, 0, nullptr);
  }
  font_atlas_bound = true;
}

// ---------------------------------------------------------------------------
// RecordCommands
// ---------------------------------------------------------------------------

auto UIRenderer::RecordCommands(VkCommandBuffer cmd_buf, u32 frame_index,
                                const UIRenderBatch &batch, u32 screen_width,
                                u32 screen_height, VulkanContext &ctx) -> void {
  if (batch.IsEmpty() || !font_atlas_bound) {
    return;
  }

  auto &fr = frame_resources[frame_index];

  // Upload vertex data
  const size_t vb_size = batch.vertices.size() * sizeof(UIVertex);
  ASSERT(vb_size <= kMaxVertices * sizeof(UIVertex),
         "UI vertex buffer overflow");
  std::memcpy(fr.vertex_mapped, batch.vertices.data(), vb_size);

  // Upload index data
  const size_t ib_size = batch.indices.size() * sizeof(u32);
  ASSERT(ib_size <= kMaxIndices * sizeof(u32), "UI index buffer overflow");
  std::memcpy(fr.index_mapped, batch.indices.data(), ib_size);

  // Use a standard (non-flipped) viewport for UI rendering so that screen Y=0
  // is the top.
  VkViewport ui_viewport{
      .x = 0.0F,
      .y = 0.0F,
      .width = static_cast<float>(screen_width),
      .height = static_cast<float>(screen_height),
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };
  vkCmdSetViewport(cmd_buf, 0, 1, &ui_viewport);

  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1, &fr.descriptor_set, 0,
                          nullptr);

  const float push[2] = {static_cast<float>(screen_width),
                         static_cast<float>(screen_height)};
  vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(push), push);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(cmd_buf, 0, 1, &fr.vertex_buffer, &offset);
  vkCmdBindIndexBuffer(cmd_buf, fr.index_buffer, 0, VK_INDEX_TYPE_UINT32);

  // Full-screen default scissor (restored after scoped scissors)
  const VkRect2D full_scissor{{0, 0}, {screen_width, screen_height}};
  vkCmdSetScissor(cmd_buf, 0, 1, &full_scissor);

  for (const auto &dc : batch.draw_calls) {
    if (dc.index_count == 0) {
      continue;
    }
    if (dc.has_scissor) {
      vkCmdSetScissor(cmd_buf, 0, 1, &dc.scissor);
    } else {
      vkCmdSetScissor(cmd_buf, 0, 1, &full_scissor);
    }
    vkCmdDrawIndexed(cmd_buf, dc.index_count, 1, dc.index_offset, 0, 0);
  }

  // Restore full-screen scissor for subsequent passes
  vkCmdSetScissor(cmd_buf, 0, 1, &full_scissor);
}

// ---------------------------------------------------------------------------
// CreateDescriptors
// ---------------------------------------------------------------------------

auto UIRenderer::CreateDescriptors(VulkanContext &ctx) -> void {
  // Descriptor set layout: binding 0 = combined image sampler (font atlas)
  VkDescriptorSetLayoutBinding binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  VkDescriptorSetLayoutCreateInfo dsl_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &binding,
  };
  ASSERT(vkCreateDescriptorSetLayout(ctx.GetDevice(), &dsl_info, nullptr,
                                     &descriptor_set_layout) == VK_SUCCESS,
         "Failed to create UI descriptor set layout");

  // Descriptor pool
  VkDescriptorPoolSize pool_size{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = MAX_FRAMES_IN_FLIGHT,
  };
  VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = 1,
      .pPoolSizes = &pool_size,
  };
  ASSERT(vkCreateDescriptorPool(ctx.GetDevice(), &pool_info, nullptr,
                                &descriptor_pool) == VK_SUCCESS,
         "Failed to create UI descriptor pool");

  // Allocate one descriptor set per frame-in-flight
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout,
    };
    ASSERT(vkAllocateDescriptorSets(ctx.GetDevice(), &alloc_info,
                                    &frame_resources[i].descriptor_set) ==
               VK_SUCCESS,
           "Failed to allocate UI descriptor set");
  }
}

// ---------------------------------------------------------------------------
// CreatePipeline
// ---------------------------------------------------------------------------

auto UIRenderer::CreatePipeline(VulkanContext &ctx,
                                const std::string &vert_spv_path,
                                const std::string &frag_spv_path,
                                VkFormat depth_format) -> void {
  auto vert_code = LoadSpirV(vert_spv_path);
  auto frag_code = LoadSpirV(frag_spv_path);

  auto make_module = [&](const std::vector<u8> &code) -> VkShaderModule {
    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const u32 *>(code.data()),
    };
    VkShaderModule mod = VK_NULL_HANDLE;
    ASSERT(vkCreateShaderModule(ctx.GetDevice(), &info, nullptr, &mod) ==
               VK_SUCCESS,
           "Failed to create UI shader module");
    return mod;
  };

  auto vert_module = make_module(vert_code);
  auto frag_module = make_module(frag_code);

  // Pipeline layout: push constant for screen size (vec2 = 8 bytes)
  VkPushConstantRange pc_range{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = sizeof(float) * 2,
  };
  VkPipelineLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pc_range,
  };
  ASSERT(vkCreatePipelineLayout(ctx.GetDevice(), &layout_info, nullptr,
                                &pipeline_layout) == VK_SUCCESS,
         "Failed to create UI pipeline layout");

  VkPipelineShaderStageCreateInfo stages[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vert_module,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = frag_module,
       .pName = "main"},
  };

  // UIVertex layout: position(2f) uv(2f) color(4f) mode(1u) = 36 bytes
  VkVertexInputBindingDescription binding_desc{
      .binding = 0,
      .stride = sizeof(UIVertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  VkVertexInputAttributeDescription attr_descs[4] = {
      {.location = 0,
       .binding = 0,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = offsetof(UIVertex, position)},
      {.location = 1,
       .binding = 0,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = offsetof(UIVertex, uv)},
      {.location = 2,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32A32_SFLOAT,
       .offset = offsetof(UIVertex, color)},
      {.location = 3,
       .binding = 0,
       .format = VK_FORMAT_R32_UINT,
       .offset = offsetof(UIVertex, mode)},
  };
  VkPipelineVertexInputStateCreateInfo vert_input{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_desc,
      .vertexAttributeDescriptionCount = 4,
      .pVertexAttributeDescriptions = attr_descs,
  };

  VkPipelineInputAssemblyStateCreateInfo input_asm{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  };

  VkPipelineViewportStateCreateInfo vp_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo raster{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0F,
  };

  VkPipelineMultisampleStateCreateInfo ms{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineDepthStencilStateCreateInfo ds{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
  };

  // Standard alpha blending: out = src.rgb * src.a + dst.rgb * (1 - src.a)
  VkPipelineColorBlendAttachmentState blend_att{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };
  VkPipelineColorBlendStateCreateInfo blend{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blend_att,
  };

  VkDynamicState dyn_states[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                  VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyn{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dyn_states,
  };

  // Dynamic rendering (no render pass object)
  const VkFormat color_fmt = ctx.GetSwapChainImageFormat();
  VkPipelineRenderingCreateInfoKHR dyn_render{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &color_fmt,
      .depthAttachmentFormat = depth_format,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  VkGraphicsPipelineCreateInfo pipe_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &dyn_render,
      .stageCount = 2,
      .pStages = stages,
      .pVertexInputState = &vert_input,
      .pInputAssemblyState = &input_asm,
      .pViewportState = &vp_state,
      .pRasterizationState = &raster,
      .pMultisampleState = &ms,
      .pDepthStencilState = &ds,
      .pColorBlendState = &blend,
      .pDynamicState = &dyn,
      .layout = pipeline_layout,
      .renderPass = VK_NULL_HANDLE,
  };

  ASSERT(vkCreateGraphicsPipelines(ctx.GetDevice(), VK_NULL_HANDLE, 1,
                                   &pipe_info, nullptr,
                                   &pipeline) == VK_SUCCESS,
         "Failed to create UI graphics pipeline");

  vkDestroyShaderModule(ctx.GetDevice(), vert_module, nullptr);
  vkDestroyShaderModule(ctx.GetDevice(), frag_module, nullptr);
}

// ---------------------------------------------------------------------------
// CreateBuffers
// ---------------------------------------------------------------------------

auto UIRenderer::CreateBuffers(VulkanContext &ctx) -> void {
  for (auto &fr : frame_resources) {
    CreateHostBuffer(ctx, kMaxVertices * sizeof(UIVertex),
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, fr.vertex_buffer,
                     fr.vertex_buffer_memory, fr.vertex_mapped);
    CreateHostBuffer(ctx, kMaxIndices * sizeof(u32),
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT, fr.index_buffer,
                     fr.index_buffer_memory, fr.index_mapped);
  }
}

} // namespace lumina::renderer
