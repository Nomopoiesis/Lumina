#include "renderer.hpp"

#include "common/logger/logger.hpp"
#include "common/lumina_assert.hpp"
#include "core/lumina_engine.hpp"
#include "shaders/shader_module_cache.hpp"

#include "math/matrix.hpp"
#include "math/vector.hpp"
#include <vulkan/vk_enum_string_helper.h>

#include <array>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lumina::renderer {

struct UniformBufferObject {
  math::Mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

struct Vertex {
  math::Vec2 position;
  math::Vec3 color;

  static auto GetBindingDescription() -> VkVertexInputBindingDescription {
    return {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
  }

  static auto GetAttributeDescriptions()
      -> std::array<VkVertexInputAttributeDescription, 2> {
    return {
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, position),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color),
        },
    };
  }
};

static const auto vertices = std::vector<Vertex>{
    {.position = {-0.5F, -0.5F}, .color = {1.0F, 0.0F, 0.0F}},
    {.position = {0.5F, -0.5F}, .color = {0.0F, 1.0F, 0.0F}},
    {.position = {0.5F, 0.5F}, .color = {0.0F, 0.0F, 1.0F}},
    {.position = {-0.5F, 0.5F}, .color = {1.0F, 1.0F, 1.0F}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

static auto
CreateDescriptorSetLayout(VulkanContext &vulkan_context,
                          VkDescriptorSetLayout &descriptor_set_layout)
    -> bool {
  VkDescriptorSetLayoutBinding ubo_layout_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = nullptr,
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = 1,
      .pBindings = &ubo_layout_binding,
  };

  if (vkCreateDescriptorSetLayout(vulkan_context.GetDevice(), &layout_info,
                                  nullptr,
                                  &descriptor_set_layout) != VK_SUCCESS) {
    LOG_ERROR("Failed to create descriptor set layout");
    return false;
  }
  return true;
}

static auto FindMemoryType(VulkanContext &vulkan_context, u32 type_filter,
                           VkMemoryPropertyFlags properties) -> u32 {
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(vulkan_context.GetPhysicalDevice(),
                                      &mem_properties);
  for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
    if (((type_filter & (1 << i)) != 0U) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }
  return 0;
}

static auto DeviceAllocateMemory(VulkanContext &vulkan_context,
                                 VkDeviceMemory &memory, VkBuffer &buffer,
                                 VkMemoryRequirements &mem_requirements)
    -> bool {
  VkMemoryAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = mem_requirements.size,
      .memoryTypeIndex =
          FindMemoryType(vulkan_context, mem_requirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };

  if (auto result =
          vkAllocateMemory(vulkan_context.GetDevice(), &info, nullptr, &memory);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to allocate memory: {}", string_VkResult(result));
    return false;
  }

  if (auto result =
          vkBindBufferMemory(vulkan_context.GetDevice(), buffer, memory, 0);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to bind buffer memory: {}", string_VkResult(result));
    return false;
  }
  return true;
}

static auto DeviceCopyBuffer(VulkanContext &vulkan_context,
                             VkCommandPool &command_pool, VkBuffer src_buffer,
                             VkBuffer dst_buffer, VkDeviceSize size) -> bool {
  VkCommandBufferAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  if (auto result = vkAllocateCommandBuffers(vulkan_context.GetDevice(), &info,
                                             &command_buffer);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to allocate command buffer for copy transfer: {}",
              string_VkResult(result));
    return false;
  }

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };

  if (auto result = vkBeginCommandBuffer(command_buffer, &begin_info);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to begin command buffer for copy transfer: {}",
              string_VkResult(result));
    return false;
  }

  VkBufferCopy copy_region = {
      .srcOffset = 0,
      .dstOffset = 0,
      .size = size,
  };
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

  if (auto result = vkEndCommandBuffer(command_buffer); result != VK_SUCCESS) {
    LOG_ERROR("Failed to end command buffer for copy transfer: {}",
              string_VkResult(result));
    return false;
  }

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = nullptr,
      .pWaitDstStageMask = nullptr,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
  };
  if (auto result = vkQueueSubmit(vulkan_context.GetGraphicsQueue(), 1,
                                  &submit_info, VK_NULL_HANDLE);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to submit command buffer for copy transfer: {}",
              string_VkResult(result));
    return false;
  }
  vkQueueWaitIdle(vulkan_context.GetGraphicsQueue());

  vkFreeCommandBuffers(vulkan_context.GetDevice(), command_pool, 1,
                       &command_buffer);

  return true;
}

static auto CreateBuffer(VulkanContext &vulkan_context, VkDeviceSize size,
                         VkBufferUsageFlags usage, VkSharingMode sharing_mode,
                         u32 queue_family_index_count,
                         const u32 *queue_family_indices,
                         VkMemoryPropertyFlags properties,
                         VkDeviceMemory &memory, VkBuffer &buffer) -> bool {
  VkBufferCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size,
      .usage = usage,
      .sharingMode = sharing_mode,
      .queueFamilyIndexCount = queue_family_index_count,
      .pQueueFamilyIndices = queue_family_indices,
  };

  if (auto result =
          vkCreateBuffer(vulkan_context.GetDevice(), &info, nullptr, &buffer);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to create buffer: {}", string_VkResult(result));
    return false;
  }

  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(vulkan_context.GetDevice(), buffer,
                                &mem_requirements);

  return DeviceAllocateMemory(vulkan_context, memory, buffer, mem_requirements);
}

static auto CreateVertexBuffer(VulkanContext &vulkan_context,
                               VkCommandPool &command_pool,
                               VkDeviceMemory &memory, VkBuffer &buffer)
    -> bool {
  VkDeviceSize size = sizeof(Vertex) * vertices.size();

  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
  if (!CreateBuffer(vulkan_context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    staging_buffer_memory, staging_buffer)) {
    throw std::runtime_error("Failed to create vertex staging buffer");
  }

  void *data = nullptr;
  vkMapMemory(vulkan_context.GetDevice(), staging_buffer_memory, 0, size, 0,
              &data);
  memcpy(data, vertices.data(), size);
  vkUnmapMemory(vulkan_context.GetDevice(), staging_buffer_memory);

  if (!CreateBuffer(vulkan_context, size,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory, buffer)) {
    throw std::runtime_error("Failed to create vertex buffer");
  }
  auto copy_result = DeviceCopyBuffer(vulkan_context, command_pool,
                                      staging_buffer, buffer, size);
  ASSERT(copy_result, "Failed to copy buffer");

  vkDestroyBuffer(vulkan_context.GetDevice(), staging_buffer, nullptr);
  vkFreeMemory(vulkan_context.GetDevice(), staging_buffer_memory, nullptr);
  return true;
}

static auto CreateIndexBuffer(VulkanContext &vulkan_context,
                              VkCommandPool &command_pool,
                              VkDeviceMemory &memory, VkBuffer &buffer)
    -> bool {
  VkDeviceSize size = sizeof(uint16_t) * indices.size();
  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
  if (!CreateBuffer(vulkan_context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    staging_buffer_memory, staging_buffer)) {
    throw std::runtime_error("Failed to create index staging buffer");
  }

  void *data = nullptr;
  vkMapMemory(vulkan_context.GetDevice(), staging_buffer_memory, 0, size, 0,
              &data);
  memcpy(data, indices.data(), size);
  vkUnmapMemory(vulkan_context.GetDevice(), staging_buffer_memory);

  if (!CreateBuffer(vulkan_context, size,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory, buffer)) {
    throw std::runtime_error("Failed to create index buffer");
  }
  auto copy_result = DeviceCopyBuffer(vulkan_context, command_pool,
                                      staging_buffer, buffer, size);
  ASSERT(copy_result, "Failed to copy buffer");

  vkDestroyBuffer(vulkan_context.GetDevice(), staging_buffer, nullptr);
  vkFreeMemory(vulkan_context.GetDevice(), staging_buffer_memory, nullptr);
  return true;
}

static auto CreateDescriptorPool(VulkanContext &vulkan_context,
                                 VkDescriptorPool &descriptor_pool,
                                 size_t descriptor_count) -> bool {
  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = descriptor_count;

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .maxSets = SafeU64ToU32(descriptor_count),
      .poolSizeCount = 1,
      .pPoolSizes = &pool_size,
  };
  if (auto result = vkCreateDescriptorPool(
          vulkan_context.GetDevice(), &pool_info, nullptr, &descriptor_pool);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to create descriptor pool: {}", string_VkResult(result));
    return false;
  }
  return true;
}

static auto CreateDescriptorSets(VulkanContext &vulkan_context,
                                 VkDescriptorPool &descriptor_pool,
                                 size_t descriptor_count,
                                 VkBuffer *uniform_buffers,
                                 VkDescriptorSetLayout &descriptor_set_layout,
                                 std::vector<VkDescriptorSet> &descriptor_sets)
    -> bool {
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts(
      descriptor_count, descriptor_set_layout);

  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = descriptor_pool,
      .descriptorSetCount = SafeU64ToU32(descriptor_count),
      .pSetLayouts = descriptor_set_layouts.data(),
  };

  descriptor_sets.resize(descriptor_count);
  if (auto result = vkAllocateDescriptorSets(
          vulkan_context.GetDevice(), &alloc_info, descriptor_sets.data());
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to allocate descriptor sets: {}",
              string_VkResult(result));
    return false;
  }

  for (size_t i = 0; i < descriptor_count; ++i) {
    VkDescriptorBufferInfo buffer_info = {
        .buffer = uniform_buffers[i],
        .offset = 0,
        .range = sizeof(UniformBufferObject),
    };
    VkWriteDescriptorSet write_descriptor_set = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[i],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &buffer_info,
        .pTexelBufferView = nullptr,
    };
    vkUpdateDescriptorSets(vulkan_context.GetDevice(), 1, &write_descriptor_set,
                           0, nullptr);
  }
  return true;
}

static auto CreateUniformBuffer(VulkanContext &vulkan_context,
                                VkDeviceMemory &memory, VkBuffer &buffer,
                                void *&mapped_data) -> bool {
  VkDeviceSize size = sizeof(UniformBufferObject);
  if (!CreateBuffer(vulkan_context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    memory, buffer)) {
    LOG_ERROR("Failed to create uniform buffer");
    return false;
  }

  vkMapMemory(vulkan_context.GetDevice(), memory, 0, size, 0, &mapped_data);
  return true;
}

static auto UpdateUniformBuffer(VulkanContext &vulkan_context, VkBuffer &buffer,
                                void *&mapped_data) -> bool {
  auto camera = core::LuminaEngine::Instance().GetCamera();
  UniformBufferObject ubo = {};
  ubo.model = math::Mat4::Identity();
  // ubo.view = camera.GetViewMatrix();
  ubo.view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  // ubo.proj = camera.GetProjectionMatrix();
  ubo.proj = glm::perspective(
      glm::radians(45.0f),
      vulkan_context.GetSwapChainImageExtent().width /
          (float)vulkan_context.GetSwapChainImageExtent().height,
      0.1f, 10.0f);
  ubo.proj[1][1] *= -1;
  memcpy(mapped_data, &ubo, sizeof(UniformBufferObject));
  return true;
}

LuminaRenderer::LuminaRenderer(platform::common::vulkan::VkInitializationResult
                                   vulkan_init_result) noexcept
    : vulkan_context(this, vulkan_init_result.instance,
                     vulkan_init_result.surface) {}

LuminaRenderer::~LuminaRenderer() noexcept {
  LOG_TRACE("Destroying Lumina Vulkan Renderer...");
  // Clear frame contexts first so semaphores/fences and frame command buffers
  // are destroyed before the command pool.
  frame_contexts.clear();

  if (descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vulkan_context.GetDevice(), descriptor_pool,
                            nullptr);
  }

  if (descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan_context.GetDevice(),
                                 descriptor_set_layout, nullptr);
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (uniform_buffers_mapped[i] != nullptr) {
      vkUnmapMemory(vulkan_context.GetDevice(), uniform_buffers_memory[i]);
    }
    if (uniform_buffers[i] != VK_NULL_HANDLE) {
      vkDestroyBuffer(vulkan_context.GetDevice(), uniform_buffers[i], nullptr);
    }
    if (uniform_buffers_memory[i] != VK_NULL_HANDLE) {
      vkFreeMemory(vulkan_context.GetDevice(), uniform_buffers_memory[i],
                   nullptr);
    }
  }

  if (descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan_context.GetDevice(),
                                 descriptor_set_layout, nullptr);
  }

  if (vertex_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(vulkan_context.GetDevice(), vertex_buffer, nullptr);
  }

  if (vertex_buffer_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), vertex_buffer_memory, nullptr);
  }

  if (index_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(vulkan_context.GetDevice(), index_buffer, nullptr);
  }

  if (index_buffer_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), index_buffer_memory, nullptr);
  }

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

  auto desc_result =
      CreateDescriptorSetLayout(vulkan_context, descriptor_set_layout);
  ASSERT(desc_result, "Failed to create descriptor set layout");

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

  auto buff_res = CreateVertexBuffer(vulkan_context, command_pool,
                                     vertex_buffer_memory, vertex_buffer);
  ASSERT(buff_res, "Failed to create vertex buffer");

  buff_res = CreateIndexBuffer(vulkan_context, command_pool,
                               index_buffer_memory, index_buffer);
  ASSERT(buff_res, "Failed to create index buffer");

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    auto uniform_buffer_result =
        CreateUniformBuffer(vulkan_context, uniform_buffers_memory[i],
                            uniform_buffers[i], uniform_buffers_mapped[i]);
    ASSERT(uniform_buffer_result, "Failed to create uniform buffer");
  }

  auto desc_pool_result = CreateDescriptorPool(vulkan_context, descriptor_pool,
                                               MAX_FRAMES_IN_FLIGHT);
  ASSERT(desc_pool_result, "Failed to create descriptor pool");
  auto desc_sets_result = CreateDescriptorSets(
      vulkan_context, descriptor_pool, MAX_FRAMES_IN_FLIGHT,
      uniform_buffers.data(), descriptor_set_layout, descriptor_sets);
  ASSERT(desc_sets_result, "Failed to create descriptor sets");

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
  // wait for the fence to be signaled - thus waiting for the previous frame
  // to be finished
  vkWaitForFences(vulkan_context.GetDevice(), 1,
                  &frame_context.GetFrameBeginReadyFence(), VK_TRUE,
                  UINT64_MAX);

  u32 image_index = 0;
  VkResult result = vkAcquireNextImageKHR(
      vulkan_context.GetDevice(), vulkan_context.GetSwapChain(), UINT64_MAX,
      frame_context.GetFrameBeginSemaphore(), VK_NULL_HANDLE, &image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    auto recreate_swap_chain_result = vulkan_context.RecreateSwapChain();
    if (!recreate_swap_chain_result) {
      LOG_ERROR("Failed to recreate swap chain: {}",
                recreate_swap_chain_result.error().message);
      throw std::runtime_error(recreate_swap_chain_result.error().message);
    }
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG_ERROR("Failed to acquire next image: {}", string_VkResult(result));
    throw std::runtime_error("Failed to acquire next image");
  }

  UpdateUniformBuffer(vulkan_context, uniform_buffers[current_frame_index],
                      uniform_buffers_mapped[current_frame_index]);

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
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      IsFramebufferResized()) {
    SetFramebufferResized(false);
    auto recreate_swap_chain_result = vulkan_context.RecreateSwapChain();
    if (!recreate_swap_chain_result) {
      LOG_ERROR("Failed to recreate swap chain: {}",
                recreate_swap_chain_result.error().message);
      throw std::runtime_error(recreate_swap_chain_result.error().message);
    }
  } else if (result != VK_SUCCESS) {
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
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout,
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

  auto binding_description = Vertex::GetBindingDescription();
  auto attribute_descriptions = Vertex::GetAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_description,
      .vertexAttributeDescriptionCount =
          static_cast<u32>(attribute_descriptions.size()),
      .pVertexAttributeDescriptions = attribute_descriptions.data(),
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

  VkBuffer vertexBuffers[] = {vertex_buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(command_buffer, 0, 1, vertexBuffers, offsets);

  VkBuffer indexBuffer[] = {index_buffer};
  vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT16);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1,
                          &descriptor_sets[current_frame_index], 0, nullptr);

  vkCmdDrawIndexed(command_buffer, static_cast<u32>(indices.size()), 1, 0, 0,
                   0);

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
