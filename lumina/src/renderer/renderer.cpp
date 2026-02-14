#include "renderer.hpp"

#include "common/logger/logger.hpp"
#include "shaders/shader_module_cache.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::renderer {

LuminaRenderer::LuminaRenderer(platform::common::vulkan::VkInitializationResult
                                   vulkan_init_result) noexcept
    : vulkan_context(vulkan_init_result.instance, vulkan_init_result.surface) {}

LuminaRenderer::~LuminaRenderer() noexcept {
  LOG_TRACE("Destroying Lumina Vulkan Renderer...");
  // Clear frame contexts first so semaphores/fences adn frame command buffers
  // are destroyed before the command pool.
  frame_contexts.clear();
  if (command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(vulkan_context.GetDevice(), command_pool, nullptr);
  }
  if (pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(vulkan_context.GetDevice(), pipeline, nullptr);
  }
  if (pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(vulkan_context.GetDevice(), pipeline_layout,
                            nullptr);
  }
}

auto LuminaRenderer::Initialize() -> void {
  LOG_TRACE("Initializing Lumina Vulkan Renderer...");
  if (!vulkan_context.Initialize()) {
    LOG_ERROR("Failed to initialize Vulkan context: {}",
              vulkan_context.Initialize().error().message);
    vulkan_context.ResetContext();
    throw std::runtime_error(vulkan_context.Initialize().error().message);
  }

  if (!CreatePipelineLayout()) {
    LOG_ERROR("Failed to create pipeline layout: {}",
              CreatePipelineLayout().error().message);
    throw std::runtime_error(CreatePipelineLayout().error().message);
  }

  if (!CreatePipeline()) {
    LOG_ERROR("Failed to create pipeline: {}",
              CreatePipeline().error().message);
    throw std::runtime_error(CreatePipeline().error().message);
  }

  auto command_pool_result = vulkan_context.CreateCommandPool();
  if (!command_pool_result) {
    LOG_ERROR("Failed to create command pool: {}",
              vulkan_context.CreateCommandPool().error().message);
    throw std::runtime_error(
        vulkan_context.CreateCommandPool().error().message);
  }
  command_pool = command_pool_result.value();

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    auto frame_context_result =
        FrameContext::Create(vulkan_context, command_pool);
    if (!frame_context_result) {
      LOG_ERROR("Failed to create frame context: {}",
                frame_context_result.error().message);
      throw std::runtime_error(frame_context_result.error().message);
    }
    frame_contexts.push_back(std::move(frame_context_result.value()));
  }
}

auto LuminaRenderer::DeviceWaitIdle() -> void {
  vkDeviceWaitIdle(vulkan_context.GetDevice());
}

auto LuminaRenderer::DrawFrame() -> void {
  auto &frame_context = frame_contexts[current_frame_index];
  // wait for the fence to be signaled - thus waiting for the previous frame to
  // be finished
  vkWaitForFences(vulkan_context.GetDevice(), 1,
                  &frame_context.GetFrameBeginReadyFence(), VK_TRUE,
                  UINT64_MAX);

  u32 image_index = 0;
  VkResult result = vkAcquireNextImageKHR(
      vulkan_context.GetDevice(), vulkan_context.GetSwapChain(), UINT64_MAX,
      frame_context.GetFrameBeginSemaphore(), VK_NULL_HANDLE, &image_index);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG_ERROR("Failed to acquire next image: {}", string_VkResult(result));
    throw std::runtime_error("Failed to acquire next image");
  }

  vkResetFences(vulkan_context.GetDevice(), 1,
                &frame_context.GetFrameBeginReadyFence());

  if (vkResetCommandBuffer(frame_context.GetCommandBuffer(), 0) != VK_SUCCESS) {
    LOG_ERROR("Failed to reset command buffer");
    throw std::runtime_error("Failed to reset command buffer");
  }

  if (auto record_command_buffer_result =
          RecordCommandBuffer(frame_context, image_index);
      !record_command_buffer_result) {
    LOG_ERROR("Failed to record command buffer: {}",
              record_command_buffer_result.error().message);
    throw std::runtime_error(record_command_buffer_result.error().message);
  }

  VkPipelineStageFlags wait_stage_flags[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame_context.GetFrameBeginSemaphore(),
      .pWaitDstStageMask = wait_stage_flags,
      .commandBufferCount = 1,
      .pCommandBuffers = &frame_context.GetCommandBuffer(),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          &vulkan_context.GetSwapChainImageReadyToPresentSemaphore(image_index),
  };

  if (vkQueueSubmit(vulkan_context.GetGraphicsQueue(), 1, &submit_info,
                    frame_context.GetFrameBeginReadyFence()) != VK_SUCCESS) {
    LOG_ERROR("Failed to submit command buffer");
    throw std::runtime_error("Failed to submit command buffer");
  }

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &vulkan_context.GetSwapChainImageReadyToPresentSemaphore(image_index),
      .swapchainCount = 1,
      .pSwapchains = &vulkan_context.GetSwapChain(),
      .pImageIndices = &image_index,
      .pResults = nullptr,
  };

  result = vkQueuePresentKHR(vulkan_context.GetPresentQueue(), &present_info);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG_ERROR("Failed to present image: {}", string_VkResult(result));
    throw std::runtime_error("Failed to present image");
  }

  current_frame_index = (current_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

auto LuminaRenderer::CreatePipelineLayout()
    -> std::expected<void, VkInitializationError> {
  VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  if (vkCreatePipelineLayout(vulkan_context.GetDevice(),
                             &pipeline_layout_create_info, nullptr,
                             &pipeline_layout) != VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create pipeline layout"});
  }

  return std::expected<void, VkInitializationError>{};
}

auto LuminaRenderer::CreatePipeline()
    -> std::expected<void, VkInitializationError> {
  ShaderModuleCache shader_module_cache(vulkan_context.GetDevice());

  auto vert_module_result = shader_module_cache.GetShaderModule(
      "C:/Projects/c++/Lumina/lumina/data/shaders/spv/shader.vert.spv",
      VK_SHADER_STAGE_VERTEX_BIT);
  if (!vert_module_result) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create vertex shader: " +
                                         vert_module_result.error().message});
  }

  auto frag_module_result = shader_module_cache.GetShaderModule(
      "C:/Projects/c++/Lumina/lumina/data/shaders/spv/shader.frag.spv",
      VK_SHADER_STAGE_FRAGMENT_BIT);
  if (!frag_module_result) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create fragment shader: " +
                                         frag_module_result.error().message});
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

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
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
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
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
  color_blend_attachment_state.blendEnable = VK_FALSE;

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
  pipeline_dynamic_rendering_info.colorAttachmentCount = 1;
  pipeline_dynamic_rendering_info.pColorAttachmentFormats =
      &vulkan_context.GetSwapChainImageFormat();

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
      .pDepthStencilState = nullptr,
      .pColorBlendState = &color_blend_state_create_info,
      .pDynamicState = &dynamic_state_create_info,
      .layout = pipeline_layout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  if (vkCreateGraphicsPipelines(vulkan_context.GetDevice(), nullptr, 1,
                                &pipeline_create_info, nullptr,
                                &pipeline) != VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create graphics pipeline"});
  }

  return std::expected<void, VkInitializationError>{};
}

auto LuminaRenderer::RecordCommandBuffer(FrameContext &frame_context,
                                         u32 image_index) noexcept
    -> std::expected<void, VkInitializationError> {
  const auto &command_buffer = frame_context.GetCommandBuffer();
  VkCommandBufferBeginInfo command_buffer_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };

  if (vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info) !=
      VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to begin command buffer"});
  }

  VkRenderingAttachmentInfoKHR rendering_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = vulkan_context.GetSwapChainImageView(image_index),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  // Transition from PRESENT_SRC_KHR (from previous frame) to
  // COLOR_ATTACHMENT_OPTIMAL
  VkImageMemoryBarrier image_memory_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = vulkan_context.GetSwapChainImage(image_index),
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &image_memory_barrier);

  auto swap_chain_image_extent = vulkan_context.GetSwapChainImageExtent();

  VkRenderingInfoKHR rendering_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .pNext = nullptr,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = swap_chain_image_extent,
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &rendering_attachment_info,
      .pDepthAttachment = nullptr,
      .pStencilAttachment = nullptr,
  };

  vkCmdBeginRendering(command_buffer, &rendering_info);

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  VkViewport viewport = {
      .x = 0.0F,
      .y = 0.0F,
      .width = static_cast<f32>(swap_chain_image_extent.width),
      .height = static_cast<f32>(swap_chain_image_extent.height),
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = swap_chain_image_extent,
  };
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  vkCmdDraw(command_buffer, 3, 1, 0, 0);

  vkCmdEndRendering(command_buffer);

  image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  image_memory_barrier.dstAccessMask = 0;
  vkCmdPipelineBarrier(command_buffer,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &image_memory_barrier);

  if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to end command buffer"});
  }

  return std::expected<void, VkInitializationError>{};
}
} // namespace lumina::renderer
