#include "renderer.hpp"

#include "common/data_structures/data_buffer.hpp"
#include "common/logger/logger.hpp"
#include "common/lumina_assert.hpp"
#include "common/lumina_check.hpp"
#include "common/lumina_terminate.hpp"
#include "common/path_registry.hpp"
#include "common/profiling/profiler.hpp"
#include "graphics_pipeline.hpp"
#include "material_instance.hpp"
#include "material_instance_handle.hpp"
#include "material_template.hpp"
#include "math/basic.hpp"
#include "shaders/shader_gen/static_shader_api.hpp"
#include "shaders/shader_module_cache.hpp"
#include "vertex_layout.hpp"
#include "vertex_serializer.hpp"
#include "vk_check.hpp"
#include "vk_helpers.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <array>
#include <limits>
#include <map>
#include <set>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

namespace lumina::renderer {

using namespace lumina::common::data_structures;

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

static auto BeginCommandBuffer(VulkanContext &vulkan_context,
                               VkCommandPool &command_pool)
    -> std::optional<VkCommandBuffer> {
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
    return std::nullopt;
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
    return std::nullopt;
  }
  return command_buffer;
}

static auto EndCommandBuffer(VulkanContext &vulkan_context,
                             VkCommandPool &command_pool,
                             VkCommandBuffer &command_buffer) -> bool {
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

static auto CopyBufferToImage(VulkanContext &vulkan_context,
                              VkCommandPool &command_pool, VkBuffer buffer,
                              VkImage image, u32 width, u32 height) -> bool {
  auto command_buffer_result = BeginCommandBuffer(vulkan_context, command_pool);
  if (!command_buffer_result) {
    return false;
  }
  auto *command_buffer = command_buffer_result.value();

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {.x = 0, .y = 0, .z = 0};
  region.imageExtent = {.width = width, .height = height, .depth = 1};

  vkCmdCopyBufferToImage(command_buffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  return EndCommandBuffer(vulkan_context, command_pool, command_buffer);
}

static auto TransitionImageLayout(VulkanContext &vulkan_context,
                                  VkCommandPool &command_pool, VkImage image,
                                  VkFormat format, VkImageLayout old_layout,
                                  VkImageLayout new_layout) -> bool {
  auto command_buffer_result = BeginCommandBuffer(vulkan_context, command_pool);
  if (!command_buffer_result) {
    return false;
  }
  auto *command_buffer = command_buffer_result.value();

  VkImageAspectFlags aspect_mask = 0;
  if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(format)) {
      aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = aspect_mask;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = 0;

  VkPipelineStageFlags source_stage = 0;
  VkPipelineStageFlags destination_stage = 0;
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    LOG_ERROR("Unsupported image layout transition");
    return false;
  }

  vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  return EndCommandBuffer(vulkan_context, command_pool, command_buffer);
}

static auto RecordDeviceCopyBufferCommands(VkCommandBuffer command_buffer,
                                           VkBuffer src_buffer,
                                           VkBuffer dst_buffer,
                                           VkDeviceSize size) -> bool {

  /*auto command_buffer_result =
      BeginCommandBuffer(vulkan_context, command_pool);
  if (!command_buffer_result) {
    return false;
  }
  auto *command_buffer = command_buffer_result.value();
*/
  VkBufferCopy copy_region = {
      .srcOffset = 0,
      .dstOffset = 0,
      .size = size,
  };
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

  // return EndCommandBuffer(vulkan_context, command_pool, command_buffer);
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

struct VulkanBufferResources {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
};

static auto RecordVertexBufferUploadCommands(VulkanContext &vulkan_context,
                                             VkCommandBuffer command_buffer,
                                             DataBufferView data_view)
    -> std::tuple<bool, VulkanBufferResources, VulkanBufferResources> {
  VkDeviceSize size = data_view.Size();

  VulkanBufferResources staging_buffer;
  if (!CreateBuffer(vulkan_context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    staging_buffer.memory, staging_buffer.buffer)) {
    // TODO: Handle error gracefully
    LOG_CRITICAL("Failed to create vertex staging buffer");
    LUMINA_TERMINATE();
  }

  void *mapped_data = nullptr;
  vkMapMemory(vulkan_context.GetDevice(), staging_buffer.memory, 0, size, 0,
              &mapped_data);
  memcpy(mapped_data, data_view.Data(), size);
  vkUnmapMemory(vulkan_context.GetDevice(), staging_buffer.memory);

  VulkanBufferResources device_buffer;
  if (!CreateBuffer(vulkan_context, size,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device_buffer.memory,
                    device_buffer.buffer)) {
    // TODO: Handle error gracefully
    LOG_CRITICAL("Failed to create vertex buffer");
    LUMINA_TERMINATE();
  }
  auto copy_result = RecordDeviceCopyBufferCommands(
      command_buffer, staging_buffer.buffer, device_buffer.buffer, size);
  LUMINA_CHECK(copy_result, "Failed to copy vertex buffer");

  return std::make_tuple(true, staging_buffer, device_buffer);
}

static auto RecordIndexBufferUploadCommands(VulkanContext &vulkan_context,
                                            VkCommandBuffer command_buffer,
                                            DataBufferView data_view)
    -> std::tuple<bool, VulkanBufferResources, VulkanBufferResources> {
  VkDeviceSize size = data_view.Size();
  VulkanBufferResources staging_buffer;
  if (!CreateBuffer(vulkan_context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    staging_buffer.memory, staging_buffer.buffer)) {
    LOG_CRITICAL("Failed to create index staging buffer");
    LUMINA_TERMINATE();
  }

  void *mapped_data = nullptr;
  vkMapMemory(vulkan_context.GetDevice(), staging_buffer.memory, 0, size, 0,
              &mapped_data);
  memcpy(mapped_data, data_view.Data(), size);
  vkUnmapMemory(vulkan_context.GetDevice(), staging_buffer.memory);

  VulkanBufferResources device_buffer;
  if (!CreateBuffer(vulkan_context, size,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device_buffer.memory,
                    device_buffer.buffer)) {
    LOG_CRITICAL("Failed to create index buffer");
    LUMINA_TERMINATE();
  }
  auto copy_result = RecordDeviceCopyBufferCommands(
      command_buffer, staging_buffer.buffer, device_buffer.buffer, size);
  LUMINA_CHECK(copy_result, "Failed to copy index buffer");

  return std::make_tuple(true, staging_buffer, device_buffer);
}

auto LuminaRenderer::CreateDescriptorPools()
    -> std::expected<void, VkInitializationError> {

  // Create persistent descriptor pool
  std::vector<VkDescriptorPoolSize> persistent_pool_sizes;
  persistent_pool_sizes.reserve(persistent_descriptor_pool_budget.size());
  for (auto &[type, count] : persistent_descriptor_pool_budget) {
    persistent_pool_sizes.push_back({
        .type = ToVkDescriptorType(type),
        .descriptorCount = SafeU64ToU32(count),
    });
  }

  if (!persistent_pool_sizes.empty()) {
    VkDescriptorPoolCreateInfo persistent_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = SafeU64ToU32(max_persistent_descriptor_sets),
        .poolSizeCount = static_cast<u32>(persistent_pool_sizes.size()),
        .pPoolSizes = persistent_pool_sizes.data(),
    };
    if (auto result = vkCreateDescriptorPool(vulkan_context.GetDevice(),
                                             &persistent_pool_info, nullptr,
                                             &persistent_descriptor_pool);
        result != VK_SUCCESS) {
      return std::unexpected(VkInitializationError{
          .message = "Failed to create persistent descriptor pool: " +
                     std::to_string(result) + ": " + string_VkResult(result)});
    }
  }

  // Create transient descriptor pool (per frame) from the global interface.
  std::vector<VkDescriptorPoolSize> transient_pool_sizes;
  GetGlobalDescriptorPoolSizes(transient_pool_sizes);

  VkDescriptorPoolCreateInfo transient_pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .maxSets = 1,
      .poolSizeCount = static_cast<u32>(transient_pool_sizes.size()),
      .pPoolSizes = transient_pool_sizes.data(),
  };
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorPool transient_descriptor_pool = VK_NULL_HANDLE;
    if (auto result = vkCreateDescriptorPool(vulkan_context.GetDevice(),
                                             &transient_pool_info, nullptr,
                                             &transient_descriptor_pool);
        result != VK_SUCCESS) {
      return std::unexpected(VkInitializationError{
          .message = "Failed to create transient descriptor pool: " +
                     std::to_string(result) + ": " + string_VkResult(result)});
    }
    frame_contexts[i]->SetTransientDescriptorPool(transient_descriptor_pool);
  }

  return {};
}

static auto CreateUniformBuffer(VulkanContext &vulkan_context,
                                VkDeviceMemory &memory, VkBuffer &buffer,
                                void *&mapped_data, VkDeviceSize size) -> bool {
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

static auto CreateInstanceBuffer(VulkanContext &vulkan_context,
                                 FrameContextInstanceBuffer &instance_buffer,
                                 u32 capacity) -> bool {
  VkDeviceSize size = capacity * sizeof(math::Mat4);
  if (!CreateBuffer(vulkan_context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    instance_buffer.memory, instance_buffer.buffer)) {
    LOG_ERROR("Failed to create instance buffer");
    return false;
  }
  vkMapMemory(vulkan_context.GetDevice(), instance_buffer.memory, 0, size, 0,
              &instance_buffer.mapped);
  instance_buffer.capacity = capacity;
  return true;
}

static auto CreateImage(VulkanContext &vulkan_context, VkImage &texture_image,
                        u32 tex_width, u32 tex_height,
                        VkDeviceMemory &texture_image_memory, VkFormat format,
                        VkImageTiling tiling, VkImageUsageFlags usage,
                        VkMemoryPropertyFlags properties) -> bool {
  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = format;
  image_info.extent = {.width = tex_width, .height = tex_height, .depth = 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = tiling;
  image_info.usage = usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (vkCreateImage(vulkan_context.GetDevice(), &image_info, nullptr,
                    &texture_image) != VK_SUCCESS) {
    LOG_ERROR("Failed to create texture image");
    return false;
  }

  VkMemoryRequirements mem_requirements;
  vkGetImageMemoryRequirements(vulkan_context.GetDevice(), texture_image,
                               &mem_requirements);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_requirements.size;
  alloc_info.memoryTypeIndex = FindMemoryType(
      vulkan_context, mem_requirements.memoryTypeBits, properties);

  if (vkAllocateMemory(vulkan_context.GetDevice(), &alloc_info, nullptr,
                       &texture_image_memory) != VK_SUCCESS) {
    LOG_ERROR("Failed to allocate memory for texture image");
    return false;
  }

  vkBindImageMemory(vulkan_context.GetDevice(), texture_image,
                    texture_image_memory, 0);
  return true;
}

static auto CreateTextureImage(VulkanContext &vulkan_context,
                               VkCommandPool &command_pool,
                               VkImage &texture_image,
                               VkDeviceMemory &texture_image_memory) -> bool {
  int tex_width = 0, tex_height = 0, tex_channels = 0;
  stbi_uc *pixels =
      stbi_load(lumina::common::PathRegistry::Instance()
                    .textures.Resolve("tex.png")
                    .string()
                    .c_str(),
                &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
  if (pixels == nullptr) {
    LOG_ERROR("Failed to load texture image");
    return false;
  }
  VkDeviceSize image_size =
      SafeI32ToU64(tex_width) * SafeI32ToU64(tex_height) * 4;

  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;

  CreateBuffer(vulkan_context, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer_memory, staging_buffer);
  void *data = nullptr;
  vkMapMemory(vulkan_context.GetDevice(), staging_buffer_memory, 0, image_size,
              0, &data);
  memcpy(data, pixels, image_size);
  vkUnmapMemory(vulkan_context.GetDevice(), staging_buffer_memory);
  stbi_image_free(pixels);

  auto format = VK_FORMAT_R8G8B8A8_SRGB;
  if (!CreateImage(vulkan_context, texture_image, SafeI32ToU32(tex_width),
                   SafeI32ToU32(tex_height), texture_image_memory, format,
                   VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
    LOG_ERROR("Failed to create texture image");
    return false;
  }

  TransitionImageLayout(vulkan_context, command_pool, texture_image, format,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CopyBufferToImage(vulkan_context, command_pool, staging_buffer, texture_image,
                    SafeI32ToU32(tex_width), SafeI32ToU32(tex_height));
  TransitionImageLayout(vulkan_context, command_pool, texture_image, format,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(vulkan_context.GetDevice(), staging_buffer, nullptr);
  vkFreeMemory(vulkan_context.GetDevice(), staging_buffer_memory, nullptr);

  return true;
}

static auto CreateTextureImageView(VulkanContext &vulkan_context,
                                   VkImage &texture_image)
    -> std::expected<VkImageView, VkInitializationError> {
  auto create_image_view_result = vulkan_context.CreateImageView(
      texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
  if (!create_image_view_result) {
    LOG_ERROR("Failed to create texture image view: {}",
              create_image_view_result.error().message);
    return std::unexpected(create_image_view_result.error());
  }
  return create_image_view_result.value();
}

static auto CreateTextureSampler(VulkanContext &vulkan_context,
                                 VkSampler &texture_sampler)
    -> std::expected<VkSampler, VkInitializationError> {
  auto physical_device_properties =
      vulkan_context.GetPhysicalDeviceProperties();
  auto max_anisotropy = physical_device_properties.limits.maxSamplerAnisotropy;
  VkSamplerCreateInfo sampler_info = {};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.anisotropyEnable = VK_TRUE;
  sampler_info.maxAnisotropy = max_anisotropy;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.mipLodBias = 0.0F;
  sampler_info.minLod = 0.0F;
  sampler_info.maxLod = 0.0F;
  if (vkCreateSampler(vulkan_context.GetDevice(), &sampler_info, nullptr,
                      &texture_sampler) != VK_SUCCESS) {
    LOG_ERROR("Failed to create texture sampler");
    return std::unexpected(
        VkInitializationError{.message = "Failed to create texture sampler"});
  }
  return texture_sampler;
}

static auto FindDepthFormat(VulkanContext &vulkan_context)
    -> std::expected<VkFormat, VkInitializationError> {
  return vulkan_context.FindSupportedFormat(
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
       VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static auto
CreateDepthResources(VulkanContext &vulkan_context, VkCommandPool &command_pool,
                     VkImage &depth_image, VkImageView &depth_image_view,
                     VkDeviceMemory &depth_image_memory,
                     VkFormat &depth_stencil_format, u32 width, u32 height)
    -> bool {
  auto depth_format = FindDepthFormat(vulkan_context);
  LUMINA_CHECK(depth_format, "Failed to find depth format");
  depth_stencil_format = depth_format.value();
  CreateImage(vulkan_context, depth_image, width, height, depth_image_memory,
              depth_stencil_format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  auto depth_image_view_result = vulkan_context.CreateImageView(
      depth_image, depth_format.value(), VK_IMAGE_ASPECT_DEPTH_BIT);
  LUMINA_CHECK(depth_image_view_result, "Failed to create depth image view");
  depth_image_view = depth_image_view_result.value();
  TransitionImageLayout(vulkan_context, command_pool, depth_image,
                        depth_stencil_format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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

  render_texture_manager.DestroyAll([this](RenderTextureHandle /*handle*/,
                                           const RenderTexture &rt) -> void {
    if (rt.sampler != VK_NULL_HANDLE) {
      vkDestroySampler(vulkan_context.GetDevice(), rt.sampler, nullptr);
    }
    if (rt.image_view != VK_NULL_HANDLE) {
      vkDestroyImageView(vulkan_context.GetDevice(), rt.image_view, nullptr);
    }
    if (rt.image != VK_NULL_HANDLE) {
      vkDestroyImage(vulkan_context.GetDevice(), rt.image, nullptr);
    }
    if (rt.memory != VK_NULL_HANDLE) {
      vkFreeMemory(vulkan_context.GetDevice(), rt.memory, nullptr);
    }
  });
  render_texture_manager.ProcessDeferredOperations();

  render_mesh_manager.DestroyAll([this](RenderMeshHandle handle,
                                        const RenderMesh &mesh) -> void {
    for (const auto &stream : mesh.vertex_streams) {
      if (stream.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vulkan_context.GetDevice(), stream.buffer, nullptr);
      }
      if (stream.memory != VK_NULL_HANDLE) {
        vkFreeMemory(vulkan_context.GetDevice(), stream.memory, nullptr);
      }
    }
    if (mesh.index_buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(vulkan_context.GetDevice(), mesh.index_buffer, nullptr);
    }
    if (mesh.index_buffer_memory != VK_NULL_HANDLE) {
      vkFreeMemory(vulkan_context.GetDevice(), mesh.index_buffer_memory,
                   nullptr);
    }
  });
  render_mesh_manager.ProcessDeferredOperations();

  if (depth_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan_context.GetDevice(), depth_image_view, nullptr);
  }
  if (depth_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan_context.GetDevice(), depth_image, nullptr);
  }
  if (depth_image_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), depth_image_memory, nullptr);
  }

  if (pick_color_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan_context.GetDevice(), pick_color_image_view,
                       nullptr);
  }
  if (pick_color_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan_context.GetDevice(), pick_color_image, nullptr);
  }
  if (pick_color_image_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), pick_color_image_memory, nullptr);
  }
  if (pick_depth_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan_context.GetDevice(), pick_depth_image_view,
                       nullptr);
  }
  if (pick_depth_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan_context.GetDevice(), pick_depth_image, nullptr);
  }
  if (pick_depth_image_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), pick_depth_image_memory, nullptr);
  }
  if (pick_readback_buffer_mapped != nullptr) {
    vkUnmapMemory(vulkan_context.GetDevice(), pick_readback_buffer_memory);
  }
  if (pick_readback_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(vulkan_context.GetDevice(), pick_readback_buffer, nullptr);
  }
  if (pick_readback_buffer_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), pick_readback_buffer_memory,
                 nullptr);
  }

  if (texture_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(vulkan_context.GetDevice(), texture_sampler, nullptr);
  }
  if (texture_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan_context.GetDevice(), texture_image_view, nullptr);
  }
  if (texture_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan_context.GetDevice(), texture_image, nullptr);
  }
  if (texture_image_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), texture_image_memory, nullptr);
  }

  if (material_ubo_mapped != nullptr) {
    vkUnmapMemory(vulkan_context.GetDevice(), material_ubo_memory);
  }
  if (material_ubo_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(vulkan_context.GetDevice(), material_ubo_buffer, nullptr);
  }
  if (material_ubo_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), material_ubo_memory, nullptr);
  }

  if (persistent_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vulkan_context.GetDevice(),
                            persistent_descriptor_pool, nullptr);
  }

  if (command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(vulkan_context.GetDevice(), command_pool, nullptr);
  }
  pipeline_manager.DestroyAll(
      [this](GraphicsPipelineHandle handle, const GraphicsPipeline &pipeline) {
        if (pipeline.vk_pipeline != VK_NULL_HANDLE) {
          vkDestroyPipeline(vulkan_context.GetDevice(), pipeline.vk_pipeline,
                            nullptr);
        }
      });
  pipeline_manager.ProcessDeferredOperations();

  // Destroy shader interfaces before the global DSL they reference.
  shader_interfaces.clear();

  if (global_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(vulkan_context.GetDevice(), global_pipeline_layout,
                            nullptr);
  }
  if (global_descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan_context.GetDevice(),
                                 global_descriptor_set_layout, nullptr);
  }
}

auto LuminaRenderer::AcquireFrameContextForUpdate() -> void {
  frames_available_for_update.acquire();
  for (auto &frame_context : frame_contexts) {
    FrameContextPipelineState expected = FrameContextPipelineState::IDLE;
    if (frame_context->pipeline_state.compare_exchange_weak(
            expected, FrameContextPipelineState::UPDATE)) {
      frame_context_for_update = frame_context.get();
      break;
    }
  }
}

auto LuminaRenderer::ReleaseFrameContextForUpdate() -> void {
  LUMINA_CHECK(frame_context_for_update, "No frame context for update");
  frame_context_for_update->pipeline_state.store(
      FrameContextPipelineState::UPDATE_COMPLETE);
  frame_context_for_update = nullptr;
  // signal that the frame context is available for render
  frames_available_for_render.release();
}

auto LuminaRenderer::AcquireFrameContextForRender() -> void {
  frames_available_for_render.acquire();
  for (auto &frame_context : frame_contexts) {
    FrameContextPipelineState expected =
        FrameContextPipelineState::UPDATE_COMPLETE;
    if (frame_context->pipeline_state.compare_exchange_weak(
            expected, FrameContextPipelineState::RENDER)) {
      frame_context_for_render = frame_context.get();
      break;
    }
  }
}

auto LuminaRenderer::ReleaseFrameContextForRender() -> void {
  LUMINA_CHECK(frame_context_for_render, "No frame context for render");
  frame_context_for_render->pipeline_state.store(
      FrameContextPipelineState::RENDER_COMPLETE);
  frame_context_for_render = nullptr;
  // We do not signal that a frame context is available for update, as it may
  // not yet be done rendering on the GPU (due to it being async), instead we
  // will signal the availability when a frame context is reclaimed
}

// Hands a frame context whose fence has signalled back to the update thread.
// Deliberately knows nothing about what that frame produced — anything the CPU
// wants to read back is claimed by ProcessReadbacks against its own sync point.
auto LuminaRenderer::ReclaimFrameContext(FrameContext &frame_context) -> void {
  frame_context.pipeline_state.store(FrameContextPipelineState::IDLE);
  frames_available_for_update.release();
}

// Completes GPU work whose result the CPU asked for.
//
// Must run above DrawFrame in the loop, because DrawFrame recycles the fence
// being polled and a result observed after that is lost. That placement is also
// *sufficient*, which is why one call site is enough: a fence is only reset for
// a context DrawFrame just acquired; that context had to be IDLE, which only
// happens via reclaim; reclaim requires the fence signalled and runs at the end
// of the loop body. So a context whose fence is reset in some iteration was
// signalled before that iteration began, and this call sees it first.
//
// Nothing here depends on the frame context that carried the work. That matters
// — the context is handed back to the update thread as soon as its fence
// signals, which can happen before the result is claimed. The readback buffer
// survives that because the engine will not issue a second pick until it has
// seen the first one's answer, so there is nothing to overwrite it.
auto LuminaRenderer::ProcessReadbacks() -> void {
  if (pending_pick_fence != VK_NULL_HANDLE &&
      vkGetFenceStatus(vulkan_context.GetDevice(), pending_pick_fence) ==
          VK_SUCCESS) {
    pending_pick_fence = VK_NULL_HANDLE;
    PublishPickFromReadback();
  }
}

auto LuminaRenderer::TryReclaimFrameContexts() -> void {
  // First check fence status of all RENDER_COMPLETE frame contexts
  size_t pending_completion = 0;
  for (auto &frame_context : frame_contexts) {
    if (frame_context->pipeline_state.load() ==
        FrameContextPipelineState::RENDER_COMPLETE) {
      auto status = vkGetFenceStatus(vulkan_context.GetDevice(),
                                     frame_context->GetFrameBeginReadyFence());
      if (status == VK_SUCCESS) {
        // The fence is signaled, so the frame context is ready to be reused
        ReclaimFrameContext(*frame_context);
        return;
      } else {
        ++pending_completion;
      }
    }
  }

  // Check if all available frame contexts are pendign completion, if so we need
  // to wait for at least one of them so that we can reclaim it and make it
  // available for update, we want to wait for the previous context as it is
  // most likely to be signaled first
  if (pending_completion == frame_contexts.size()) {
    auto ctx_idx = (current_frame_index - 1) % MAX_FRAMES_IN_FLIGHT;
    LUMINA_CHECK(ctx_idx < frame_contexts.size(),
                 "Frame context index out of bounds");
    vkWaitForFences(vulkan_context.GetDevice(), 1,
                    &frame_contexts[ctx_idx]->GetFrameBeginReadyFence(),
                    VK_TRUE, UINT64_MAX);
    ReclaimFrameContext(*frame_contexts[ctx_idx]);
  }
}

auto LuminaRenderer::ProcessDeferredOperations() -> void {
  render_texture_manager.ProcessDeferredOperations();
  render_mesh_manager.ProcessDeferredOperations();
  material_template_manager.ProcessDeferredOperations();
  material_instance_manager.ProcessDeferredOperations();
  pipeline_manager.ProcessDeferredOperations();
}

auto LuminaRenderer::RenderThread() -> void {
  platform::common::PlatformServices::Instance().LuminaSetThreadName(
      "LuminaRendererThread");
  while (!shutdown_requested) {
    PollAndExecuteCommandContexts();
    // Must stay above DrawFrame, which recycles frame fences. See its note for
    // why that placement is sufficient as well as necessary.
    ProcessReadbacks();
    AcquireFrameContextForRender();
    DrawFrame();
    ProcessDeferredOperations();
    ReleaseFrameContextForRender();
    TryReclaimFrameContexts();
  }
  // Drain all in-flight GPU work before the thread exits so that Shutdown()
  // can safely destroy GPU resources without a separate DeviceWaitIdle call.
  vkDeviceWaitIdle(vulkan_context.GetDevice());
}

auto LuminaRenderer::PollAndExecuteCommandContexts() -> void {
  auto size = global_submission_queue.Size();
  std::vector<CommandContext *> cmd_ctxs(size);
  for (size_t i = 0; i < size; ++i) {
    auto res = global_submission_queue.Pop(cmd_ctxs[i]);
    LUMINA_CHECK(res, "Failed to pop command context from submission queue");
  }

  if (!cmd_ctxs.empty()) {
    std::vector<VkCommandBuffer> command_buffers(cmd_ctxs.size());
    for (size_t i = 0; i < cmd_ctxs.size(); ++i) {
      command_buffers[i] = cmd_ctxs[i]->command_buffer;
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = static_cast<uint32_t>(cmd_ctxs.size()),
        .pCommandBuffers = command_buffers.data(),
    };

    auto submission_fence_result = vulkan_context.CreateFence(false);
    if (!submission_fence_result) {
      LOG_CRITICAL("Failed to create submission fence: {}",
                   submission_fence_result.error().message);
      LUMINA_TERMINATE();
    }
    auto *submission_fence = submission_fence_result.value();

    if (auto result = vkQueueSubmit(vulkan_context.GetGraphicsQueue(), 1,
                                    &submit_info, submission_fence);
        result != VK_SUCCESS) {
      LOG_ERROR("Failed to submit command buffer for copy transfer: {}",
                string_VkResult(result));
      LUMINA_TERMINATE();
    }

    pending_submissions.emplace_back(submission_fence, std::move(cmd_ctxs));
  }

  for (auto it = pending_submissions.begin();
       it != pending_submissions.end();) {
    auto &[fence, submitted_ctxs] = *it;
    auto status = vkGetFenceStatus(vulkan_context.GetDevice(), fence);
    if (status == VK_SUCCESS) {
      vulkan_context.DestroyFence(fence);
      for (auto &cmd_ctx : submitted_ctxs) {
        cmd_ctx->RunCompletionCallbacks();
        cmd_ctx->Reset();
        ReleaseCommandContext(*cmd_ctx);
      }
      it = pending_submissions.erase(it);
    } else {
      ++it;
    }
  }
}

auto LuminaRenderer::Initialize() -> void {
  LOG_TRACE("Initializing Lumina Vulkan Renderer...");
  auto vulkan_context_result = vulkan_context.Initialize();
  if (!vulkan_context_result) {
    LOG_CRITICAL("Failed to initialize Vulkan context: {}",
                 vulkan_context_result.error().message);
    LUMINA_TERMINATE();
  }

  command_context_pool.Initialize(
      32, [this](auto &ctx) { ctx.Initialize(vulkan_context); });

  auto command_pool_result = vulkan_context.CreateCommandPool();
  if (!command_pool_result) {
    LOG_CRITICAL("Failed to create command pool: {}",
                 command_pool_result.error().message);
    LUMINA_TERMINATE();
  }
  command_pool = command_pool_result.value();

  auto global_dsl_result = CreateGlobalDescriptorSetLayout(this);
  if (!global_dsl_result) {
    LOG_CRITICAL("Failed to create global descriptor set layout: {}",
                 global_dsl_result.error().message);
    LUMINA_TERMINATE();
  }

  BuildStaticMaterialTemplates(this);

  default_material_instance_handle =
      CreateMaterialInstance(default_material_template_handle);

  ProcessDeferredOperations();

  auto depth_result = CreateDepthResources(
      vulkan_context, command_pool, depth_image, depth_image_view,
      depth_image_memory, depth_stencil_format,
      vulkan_context.GetSwapChainImageExtent().width,
      vulkan_context.GetSwapChainImageExtent().height);
  LUMINA_CHECK(depth_result, "Failed to create depth resources");

  auto res = CreateTextureImage(vulkan_context, command_pool, texture_image,
                                texture_image_memory);
  LUMINA_CHECK(res, "Failed to create texture image");
  auto texture_image_view_result =
      CreateTextureImageView(vulkan_context, texture_image);
  LUMINA_CHECK(texture_image_view_result,
               "Failed to create texture image view");
  texture_image_view = texture_image_view_result.value();

  auto texture_sampler_result =
      CreateTextureSampler(vulkan_context, texture_sampler);
  LUMINA_CHECK(texture_sampler_result, "Failed to create texture sampler");
  texture_sampler = texture_sampler_result.value();

  VkDeviceSize pick_readback_buffer_size =
      sizeof(u32) * PICK_REGION_SIZE * PICK_REGION_SIZE;
  auto pick_readback_buffer_result = CreateBuffer(
      vulkan_context, pick_readback_buffer_size,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      pick_readback_buffer_memory, pick_readback_buffer);
  LUMINA_CHECK(pick_readback_buffer_result,
               "Failed to create pick readback buffer");
  vkMapMemory(vulkan_context.GetDevice(), pick_readback_buffer_memory, 0,
              pick_readback_buffer_size, 0, &pick_readback_buffer_mapped);
  auto pick_color_image_result = CreateImage(
      vulkan_context, pick_color_image, PICK_REGION_SIZE, PICK_REGION_SIZE,
      pick_color_image_memory, VK_FORMAT_R32_UINT, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  LUMINA_CHECK(pick_color_image_result, "Failed to create pick color image");
  // Not CreateTextureImageView: that helper hardcodes R8G8B8A8_SRGB, and a view
  // whose format disagrees with its image is invalid.
  auto pick_color_image_view_result = vulkan_context.CreateImageView(
      pick_color_image, VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT);
  LUMINA_CHECK(pick_color_image_view_result,
               "Failed to create pick color image view");
  pick_color_image_view = pick_color_image_view_result.value();
  auto pick_depth_image_result = CreateDepthResources(
      vulkan_context, command_pool, pick_depth_image, pick_depth_image_view,
      pick_depth_image_memory, depth_stencil_format, PICK_REGION_SIZE,
      PICK_REGION_SIZE);
  LUMINA_CHECK(pick_depth_image_result, "Failed to create pick depth image");

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    auto frame_context_result =
        FrameContext::Create(vulkan_context, command_pool);
    if (!frame_context_result) {
      LOG_CRITICAL("Failed to create frame context: {}",
                   frame_context_result.error().message);
      LUMINA_TERMINATE();
    }
    frame_contexts.push_back(std::move(frame_context_result.value()));
    auto &uniform_buffer = frame_contexts[i]->GetUniformBuffer();
    auto uniform_buffer_result = CreateUniformBuffer(
        vulkan_context, uniform_buffer.memory, uniform_buffer.buffer,
        uniform_buffer.mapped, GetFrameGlobalsBufferSize());
    LUMINA_CHECK(uniform_buffer_result, "Failed to create uniform buffer");
    auto instance_buffer_result = CreateInstanceBuffer(
        vulkan_context, frame_contexts[i]->GetInstanceBuffer(), 4096);
    LUMINA_CHECK(instance_buffer_result, "Failed to create instance buffer");
  }

  auto desc_pool_result = CreateDescriptorPools();
  if (!desc_pool_result) {
    LOG_CRITICAL("Failed to create descriptor pools: {}",
                 desc_pool_result.error().message);
    LUMINA_TERMINATE();
  }
  // Create the shared material uniform buffer: one slot per instance the
  // material templates collectively budgeted for.
  if (material_instance_budget > 0) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(vulkan_context.GetPhysicalDevice(),
                                  &device_properties);

    // Descriptor buffer offsets must be multiples of this, so the per-instance
    // stride is the uniform block rounded up rather than its bare size.
    const VkDeviceSize alignment =
        device_properties.limits.minUniformBufferOffsetAlignment;
    const VkDeviceSize uniform_size = GetDefaultMaterialUBOSize();
    material_ubo_slot_stride =
        (alignment == 0)
            ? uniform_size
            : ((uniform_size + alignment - 1) / alignment) * alignment;

    const VkDeviceSize buffer_size =
        material_ubo_slot_stride * material_instance_budget;
    CreateBuffer(vulkan_context, buffer_size,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
                 0, nullptr,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 material_ubo_memory, material_ubo_buffer);
    vkMapMemory(vulkan_context.GetDevice(), material_ubo_memory, 0, buffer_size,
                0, &material_ubo_mapped);

    // Every slot gets defaults, not just the ones in use, so an instance can
    // never be drawn with uninitialised uniforms.
    for (u32 slot = 0; slot < material_instance_budget; ++slot) {
      InitDefaultMaterialUBO(GetMaterialUniformData(slot));
    }
  }

  AllocatePersistentDescriptorSets(default_material_instance_handle);
  ProcessDeferredOperations();
  WriteLitMaterialDescriptors(this, default_material_instance_handle);

  {
    auto mat_opt =
        material_instance_manager.Get(default_material_instance_handle);
    LUMINA_CHECK(mat_opt.has_value(), "Default material instance not found");
    auto pipeline_desc = GraphicsPipelineDesc{
        .vertex_layout = VertexBufferLayout::Interleave(
            std::span<const core::VertexAttribute>(
                {core::VertexAttribute{.type =
                                           core::VertexAttributeType::Position,
                                       .element_type = core::ElementType::Vec3},
                 core::VertexAttribute{.type =
                                           core::VertexAttributeType::Normal,
                                       .element_type = core::ElementType::Vec3},
                 core::VertexAttribute{
                     .type = core::VertexAttributeType::TexCoord,
                     .element_type = core::ElementType::Vec2}})),
        .material_template = mat_opt.value()->GetTemplateHandle(),
        .color_attachment_formats = {vulkan_context.GetSwapChainImageFormat()},
        .depth_stencil_attachment_format = depth_stencil_format};
    auto default_pipeline_handle_result = CreateGraphicsPipeline(pipeline_desc);
    if (!default_pipeline_handle_result) {
      LOG_CRITICAL("Failed to create default pipeline: {}",
                   default_pipeline_handle_result.error().Message());
      LUMINA_TERMINATE();
    }
    default_pipeline_handle = default_pipeline_handle_result.value();
    ProcessDeferredOperations();
  }

  {
    auto pipeline_desc = GraphicsPipelineDesc{
        .vertex_layout = VertexBufferLayout::Interleave(
            std::span<const core::VertexAttribute>({core::VertexAttribute{
                .type = core::VertexAttributeType::Position,
                .element_type = core::ElementType::Vec3}})),
        .material_template = debug_wireframe_material_template_handle,
        .topology = PrimitiveTopology::LineList,
        .color_attachment_formats = {vulkan_context.GetSwapChainImageFormat()},
        .depth_stencil_attachment_format = depth_stencil_format};
    auto debug_aabb_pipeline_handle_result =
        CreateGraphicsPipeline(pipeline_desc);
    if (!debug_aabb_pipeline_handle_result) {
      LOG_CRITICAL("Failed to create debug AABB pipeline: {}",
                   debug_aabb_pipeline_handle_result.error().Message());
      LUMINA_TERMINATE();
    }
    debug_aabb_pipeline_handle = debug_aabb_pipeline_handle_result.value();
    ProcessDeferredOperations();
  }

  {
    // The full interleaved scene layout, not a position-only one: the pick pass
    // draws the ordinary scene meshes, so the binding stride and attribute
    // offsets have to describe their vertex buffers. The shader reads only
    // location 0; ToVkAttributeDescriptions skips the rest.
    //
    // This does mean the pick pipeline assumes the default vertex layout. A
    // mesh built with a different one needs its own pick pipeline variant.
    auto pipeline_desc = GraphicsPipelineDesc{
        .vertex_layout = VertexBufferLayout::Interleave(
            std::span<const core::VertexAttribute>(
                {core::VertexAttribute{.type =
                                           core::VertexAttributeType::Position,
                                       .element_type = core::ElementType::Vec3},
                 core::VertexAttribute{.type =
                                           core::VertexAttributeType::Normal,
                                       .element_type = core::ElementType::Vec3},
                 core::VertexAttribute{
                     .type = core::VertexAttributeType::TexCoord,
                     .element_type = core::ElementType::Vec2}})),
        .material_template = pick_id_material_template_handle,
        .color_attachment_formats = {VK_FORMAT_R32_UINT},
        .depth_stencil_attachment_format = depth_stencil_format};
    auto pick_pipeline_handle_result = CreateGraphicsPipeline(pipeline_desc);
    if (!pick_pipeline_handle_result) {
      LOG_CRITICAL("Failed to create pick pipeline: {}",
                   pick_pipeline_handle_result.error().Message());
      LUMINA_TERMINATE();
    }
    pick_pipeline_handle = pick_pipeline_handle_result.value();
    ProcessDeferredOperations();
  }

  ui_renderer.Initialize(vulkan_context,
                         lumina::common::PathRegistry::Instance()
                             .shaders.Resolve("ui.vert.spv")
                             .string(),
                         lumina::common::PathRegistry::Instance()
                             .shaders.Resolve("ui.frag.spv")
                             .string(),
                         depth_stencil_format);

  render_thread = std::thread(&LuminaRenderer::RenderThread, this);
}

auto LuminaRenderer::Shutdown() -> void {
  shutdown_requested = true;
  render_thread.join(); // GPU is idle when this returns
  ui_renderer.Shutdown(vulkan_context);
}

auto LuminaRenderer::DeviceWaitIdle() -> void {
  vkDeviceWaitIdle(vulkan_context.GetDevice());
}

auto LuminaRenderer::DrawUI(FrameContext &frame_context,
                            VkCommandBuffer command_buffer, u32 image_index)
    -> void {

  auto swap_chain_image_extent = vulkan_context.GetSwapChainImageExtent();
  VkRenderingAttachmentInfo color_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = vulkan_context.GetSwapChainImageView(image_index),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  VkRenderingInfo rendering_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .pNext = nullptr,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = swap_chain_image_extent,
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
      .pDepthAttachment = nullptr,
      .pStencilAttachment = nullptr,
  };
  vkCmdBeginRendering(command_buffer, &rendering_info);

  ui_renderer.RecordCommands(
      command_buffer, static_cast<u32>(current_frame_index),
      frame_context.ui_batch, swap_chain_image_extent.width,
      swap_chain_image_extent.height, vulkan_context);

  vkCmdEndRendering(command_buffer);
}

auto LuminaRenderer::DrawFrame() -> void {
  auto &frame_context = *frame_context_for_render;

  // Recycles this context's fence. ProcessReadbacks, at the top of the render
  // loop, is guaranteed to have claimed anything waiting on it first — see the
  // note there.
  vkResetFences(vulkan_context.GetDevice(), 1,
                &frame_context.GetFrameBeginReadyFence());

  PrepareFrameDescriptors(frame_context);

  u32 image_index = 0;
  VkResult result = vkAcquireNextImageKHR(
      vulkan_context.GetDevice(), vulkan_context.GetSwapChain(), UINT64_MAX,
      frame_context.GetFrameBeginSemaphore(), VK_NULL_HANDLE, &image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // This frame is abandoned before its command buffer is ever recorded or
    // submitted, so a pick riding on it never reaches the readback buffer and
    // no fence will carry it. The engine will not issue another until it sees a
    // result, so publishing one here is what stops a single window resize from
    // wedging picking for the rest of the run.
    if (!frame_context.pick_draws.empty()) {
      PublishPickResult(0);
    }
    auto recreate_swap_chain_result = vulkan_context.RecreateSwapChain();
    if (!recreate_swap_chain_result) {
      LOG_CRITICAL("Failed to recreate swap chain: {}",
                   recreate_swap_chain_result.error().message);
      LUMINA_TERMINATE();
    }
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG_CRITICAL("Failed to acquire next image: {}", string_VkResult(result));
    LUMINA_TERMINATE();
  }

  if (vkResetCommandBuffer(frame_context.GetCommandBuffer(), 0) != VK_SUCCESS) {
    LOG_CRITICAL("Failed to reset command buffer");
    LUMINA_TERMINATE();
  }

  const auto &command_buffer = frame_context.GetCommandBuffer();
  VkCommandBufferBeginInfo command_buffer_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };
  if (vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info) !=
      VK_SUCCESS) {
    LOG_CRITICAL("Failed to begin command buffer");
    LUMINA_TERMINATE();
  }

  if (auto record_command_buffer_result =
          RecordCommandBuffer(frame_context, command_buffer, image_index);
      !record_command_buffer_result) {
    LOG_CRITICAL("Failed to record command buffer: {}",
                 record_command_buffer_result.error().message);
    LUMINA_TERMINATE();
  }

  // Its own attachments, so it is independent of the scene and UI passes and
  // only needs to be inside this command buffer. No-op on frames without a
  // pending pick.
  RecordPickPass(frame_context, command_buffer);

  DrawUI(frame_context, command_buffer, image_index);

  VkImageMemoryBarrier image_memory_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = 0,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
  vkCmdPipelineBarrier(command_buffer,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &image_memory_barrier);

  if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
    LOG_CRITICAL("Failed to end command buffer");
    LUMINA_TERMINATE();
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
    LOG_CRITICAL("Failed to submit command buffer");
    LUMINA_TERMINATE();
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
      LOG_CRITICAL("Failed to recreate swap chain: {}",
                   recreate_swap_chain_result.error().message);
      LUMINA_TERMINATE();
    }
  } else if (result != VK_SUCCESS) {
    LOG_CRITICAL("Failed to present image: {}", string_VkResult(result));
    LUMINA_TERMINATE();
  }

  current_frame_index = (current_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

auto LuminaRenderer::CreateGraphicsPipeline(const GraphicsPipelineDesc &desc)
    -> std::expected<GraphicsPipelineHandle, common::LuminaError> {
  auto material_template_opt =
      material_template_manager.Get(desc.material_template);
  if (!material_template_opt) {
    return std::unexpected(common::LuminaError(
        "CreateGraphicsPipeline: Material template not found"));
  }
  auto &material_template = material_template_opt.value();
  auto &shader_interface = GetShaderInterfaceFor(*material_template);
  auto pipeline_result = ::lumina::renderer::CreateGraphicsPipeline(
      vulkan_context, *material_template, shader_interface, desc);
  if (!pipeline_result) {
    return std::unexpected(pipeline_result.error());
  }
  auto &pipeline = pipeline_result.value();
  return pipeline_manager.Create(std::move(pipeline));
}

auto LuminaRenderer::PrepareFrameDescriptors(FrameContext &frame_context)
    -> void {
  auto *device = vulkan_context.GetDevice();
  auto *pool = frame_context.GetTransientDescriptorPool();

  // Reset the transient pool
  vkResetDescriptorPool(device, pool, 0);

  // Allocate a single descriptor set for set 0 (per-frame globals).
  LUMINA_CHECK(global_descriptor_set_layout != VK_NULL_HANDLE,
               "Global descriptor set layout not created");
  auto dsl = global_descriptor_set_layout;

  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &dsl,
  };

  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set),
           "Failed to allocate transient descriptor set");

  WriteTransientDescriptors(this, frame_context, descriptor_set);

  frame_context.SetTransientDescriptorSet(descriptor_set);
}

auto LuminaRenderer::AllocatePersistentDescriptorSets(
    MaterialInstanceHandle instance_handle) -> bool {
  auto instance_opt = material_instance_manager.Get(instance_handle);
  if (!instance_opt.has_value()) {
    LOG_CRITICAL("Material instance not found");
    LUMINA_TERMINATE();
  }
  auto &instance = instance_opt.value();
  auto tmpl_opt = material_template_manager.Get(instance->GetTemplateHandle());
  if (!tmpl_opt) {
    LOG_CRITICAL("Material template not found");
    LUMINA_TERMINATE();
  }
  auto &tmpl = tmpl_opt.value();
  auto *device = vulkan_context.GetDevice();
  auto &interface = GetShaderInterfaceFor(*tmpl);

  // Get the descriptor set layout for set 1 (per-material).
  auto *dsl = interface.GetDescriptorSetLayout(1);
  if (dsl == VK_NULL_HANDLE) {
    LOG_WARNING("No descriptor set layout for set 1 — material has no "
                "persistent bindings");
    return true;
  }

  // Allocate MAX_FRAMES_IN_FLIGHT sets from the persistent pool.
  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
  layouts.fill(dsl);

  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = persistent_descriptor_pool,
      .descriptorSetCount = static_cast<u32>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data(),
  };

  auto descriptor_sets = instance->GetDescriptorSets();
  auto result =
      vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets.data());
  if (result != VK_SUCCESS) {
    LOG_ERROR("Failed to allocate persistent descriptor sets: {}",
              string_VkResult(result));
    return false;
  }

  material_instance_manager.Update(
      instance_handle,
      [descriptor_sets](MaterialInstance &material_instance) -> void {
        material_instance.GetDescriptorSetsMutable() = descriptor_sets;
      });

  return true;
}

auto LuminaRenderer::FreePersistentDescriptorSets(
    MaterialInstanceHandle instance_handle) -> void {
  auto instance_opt = material_instance_manager.Get(instance_handle);
  if (!instance_opt.has_value()) {
    LOG_CRITICAL("Material instance not found");
    LUMINA_TERMINATE();
  }
  auto &instance = instance_opt.value();
  const auto &descriptor_sets = instance->GetDescriptorSets();

  // Check if any sets were actually allocated.
  if (descriptor_sets[0] == VK_NULL_HANDLE) {
    return;
  }

  auto patch = [this](MaterialInstance &material_instance) -> void {
    vkFreeDescriptorSets(vulkan_context.GetDevice(), persistent_descriptor_pool,
                         static_cast<u32>(MAX_FRAMES_IN_FLIGHT),
                         material_instance.GetDescriptorSets().data());
    material_instance.ResetDescriptorSets();
  };
  material_instance_manager.Update(instance_handle, patch);
}

auto LuminaRenderer::AccumulateDescriptorBudget(const ShaderLayout &vert_layout,
                                                const ShaderLayout &frag_layout,
                                                size_t max_instances) -> void {
  // Merge bindings by (set, binding) key to avoid double-counting shared
  // bindings between vertex and fragment stages.
  std::map<std::pair<u32, u32>, DescriptorBindingType> merged;
  for (u32 i = 0; i < vert_layout.binding_count; ++i) {
    const auto &b = vert_layout.bindings[i];
    merged[{b.set, b.binding}] = b.type;
  }
  for (u32 i = 0; i < frag_layout.binding_count; ++i) {
    const auto &b = frag_layout.bindings[i];
    merged[{b.set, b.binding}] = b.type;
  }

  // Accumulate into persistent budget (set 0 is handled globally).
  std::set<u32> persistent_sets;
  for (const auto &[key, type] : merged) {
    auto [set, binding] = key;
    if (set == 0) {
      continue;
    }
    persistent_descriptor_pool_budget[type] +=
        max_instances * MAX_FRAMES_IN_FLIGHT;
    persistent_sets.insert(set);
  }

  max_persistent_descriptor_sets +=
      max_instances * persistent_sets.size() * MAX_FRAMES_IN_FLIGHT;

  // Uniform slots come from the same declaration as the descriptor budget, so
  // raising max_instances grows both and they cannot fall out of step.
  material_instance_budget += static_cast<u32>(max_instances);
}

auto LuminaRenderer::CreateMaterialTemplate(
    const MaterialTemplateDescription &desc) -> MaterialTemplateHandle {
  auto material_template_result = MaterialTemplate::Create(
      desc.vertex_shader_bin_path, desc.fragment_shader_bin_path,
      desc.shader_interface_index);
  if (!material_template_result) {
    LOG_CRITICAL("Failed to create material template: {}",
                 material_template_result.error().message);
    LUMINA_TERMINATE();
  }
  auto &material_template = material_template_result.value();

  AccumulateDescriptorBudget(desc.vertex_layout, desc.fragment_layout,
                             desc.max_instances);

  auto material_template_handle =
      material_template_manager.Create(std::move(material_template));
  return material_template_handle;
}

auto LuminaRenderer::CreateMaterialInstance(MaterialTemplateHandle tmpl_handle)
    -> MaterialInstanceHandle {
  auto material_instance_result = MaterialInstance::Create(tmpl_handle);
  if (!material_instance_result) {
    LOG_CRITICAL("Failed to create material instance: {}",
                 material_instance_result.error().message);
    LUMINA_TERMINATE();
  }
  LUMINA_CHECK(next_material_ubo_slot < material_instance_budget,
               "Material instance limit reached — raise max_instances on the "
               "material template description");
  material_instance_result.value().SetUniformSlot(next_material_ubo_slot++);

  auto material_instance_handle = material_instance_manager.Create(
      std::move(material_instance_result.value()));
  return material_instance_handle;
}

auto LuminaRenderer::RecordCommandBuffer(FrameContext &frame_context,
                                         VkCommandBuffer command_buffer,
                                         u32 image_index) noexcept
    -> std::expected<void, VkInitializationError> {
  // Detached: this runs on the render thread, so its span can straddle the
  // update thread's frame boundary and it must stay out of the accounted /
  // unaccounted arithmetic. Read it as "render thread work near this frame".
  LUMINA_PROFILE_SCOPE_DETACHED("Record");

  VkImageMemoryBarrier depth_image_memory_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = depth_image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
                            (HasStencilComponent(depth_stencil_format)
                                 ? VK_IMAGE_ASPECT_STENCIL_BIT
                                 : 0U),
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  vkCmdPipelineBarrier(command_buffer,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &depth_image_memory_barrier);

  // Transition from VK_IMAGE_LAYOUT_UNDEFINED (from previous frame) to
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
  VkRenderingAttachmentInfo color_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = vulkan_context.GetSwapChainImageView(image_index),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  VkRenderingAttachmentInfo depth_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = depth_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {.depthStencil = {.depth = 1.0F, .stencil = 0}},
  };

  VkRenderingInfo rendering_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .pNext = nullptr,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = swap_chain_image_extent,
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
      .pDepthAttachment = &depth_attachment_info,
      .pStencilAttachment = HasStencilComponent(depth_stencil_format)
                                ? &depth_attachment_info
                                : nullptr,
  };

  vkCmdBeginRendering(command_buffer, &rendering_info);

  VkViewport viewport = {
      .x = 0.0F,
      .y = static_cast<f32>(swap_chain_image_extent.height),
      .width = static_cast<f32>(swap_chain_image_extent.width),
      .height = -static_cast<f32>(swap_chain_image_extent.height),
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = swap_chain_image_extent,
  };
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, global_pipeline_layout,
      0, 1, &frame_context.GetTransientDescriptorSet(), 0, nullptr);

  // Accumulated locally and published once below: an atomic increment per draw
  // would put contended traffic in the hottest loop in the renderer.
  u32 draw_calls = 0;

  for (const auto &batch : frame_context.draw_batches) {
    const auto draw_item = draw_item_registry.Get(batch.draw_item_index);

    auto render_mesh_opt =
        render_mesh_manager.Get(draw_item.render_mesh_handle);
    if (!render_mesh_opt.has_value() || !render_mesh_opt.value()->ready) {
      continue;
    }
    const auto &render_mesh = render_mesh_opt.value();

    auto pipeline_opt = pipeline_manager.Get(draw_item.pipeline_handle);
    if (!pipeline_opt.has_value()) {
      continue;
    }
    auto &pipeline = pipeline_opt.value();

    auto material_instance_opt =
        material_instance_manager.Get(draw_item.material_instance_handle);
    if (!material_instance_opt.has_value()) {
      continue;
    }
    auto &material_instance = material_instance_opt.value();

    auto material_template_opt =
        material_template_manager.Get(material_instance->GetTemplateHandle());
    if (!material_template_opt.has_value()) {
      continue;
    }
    auto &material_template = material_template_opt.value();
    auto &si = GetShaderInterfaceFor(*material_template);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->vk_pipeline);
    vkCmdBindDescriptorSets(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, si.GetPipelineLayout(),
        1, 1, &material_instance->GetDescriptorSet(current_frame_index), 0,
        nullptr);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1,
                           &render_mesh->vertex_streams[0].buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, render_mesh->index_buffer, 0,
                         VK_INDEX_TYPE_UINT16);

    // No push constant: the vertex shader indexes the instance buffer with
    // gl_InstanceIndex, which Vulkan defines as firstInstance + instance
    // number, so first_instance selects this batch's slice for free.
    vkCmdDrawIndexed(command_buffer, static_cast<u32>(render_mesh->index_count),
                     batch.instance_count, 0, 0, batch.first_instance);
    ++draw_calls;
  }

  for (const auto &debug_draw : frame_context.debug_draws) {
    auto pipeline_opt = pipeline_manager.Get(debug_aabb_pipeline_handle);
    auto render_mesh_opt =
        render_mesh_manager.Get(debug_draw.render_mesh_handle);
    if (!pipeline_opt.has_value() || !render_mesh_opt.has_value() ||
        !render_mesh_opt.value()->ready) {
      continue;
    }
    auto &pipeline = pipeline_opt.value();
    const auto &render_mesh = render_mesh_opt.value();

    auto material_template_opt =
        material_template_manager.Get(debug_wireframe_material_template_handle);
    if (!material_template_opt.has_value()) {
      continue;
    }
    auto &material_template = material_template_opt.value();
    auto &dbg_si = GetShaderInterfaceFor(*material_template);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->vk_pipeline);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1,
                           &render_mesh->vertex_streams[0].buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, render_mesh->index_buffer, 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(command_buffer, dbg_si.GetPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(math::Mat4),
                       &debug_draw.model);
    vkCmdDrawIndexed(command_buffer, static_cast<u32>(render_mesh->index_count),
                     1, 0, 0, 0);
    ++draw_calls;
  }

  recorded_draw_call_count.store(draw_calls, std::memory_order_relaxed);

  vkCmdEndRendering(command_buffer);

  return std::expected<void, VkInitializationError>{};
}

auto LuminaRenderer::AcquireCommandContext() -> CommandContext & {
  auto *cmd_ctx = command_context_pool.Acquire();
  LUMINA_CHECK(cmd_ctx != nullptr, "Command context pool exhausted");
  return *cmd_ctx;
}

auto LuminaRenderer::ReleaseCommandContext(CommandContext &cmd_ctx) -> void {
  command_context_pool.Release(&cmd_ctx);
}

auto LuminaRenderer::RecordPickPass(FrameContext &frame_context,
                                    VkCommandBuffer command_buffer) -> void {
  const auto &draws = frame_context.pick_draws;
  if (draws.empty()) {
    return;
  }

  auto pipeline_opt = pipeline_manager.Get(pick_pipeline_handle);
  auto material_template_opt =
      material_template_manager.Get(pick_id_material_template_handle);
  if (!pipeline_opt.has_value() || !material_template_opt.has_value()) {
    LOG_WARNING("Pick requested before the pick pipeline was ready");
    // Nothing is recorded, so no fence will ever carry this result. The engine
    // will not issue another pick until it sees one, so skipping this wedges
    // picking permanently and silently.
    PublishPickResult(0);
    return;
  }
  auto &pipeline = pipeline_opt.value();
  auto *pipeline_layout =
      GetShaderInterfaceFor(*material_template_opt.value()).GetPipelineLayout();

  // Both attachments are fully overwritten by the clear, so discarding whatever
  // the previous pick left in them is exactly what UNDEFINED is for.
  VkImageMemoryBarrier attachment_barriers[] = {
      {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = pick_color_image,
          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      },
      {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = pick_depth_image,
          .subresourceRange = {static_cast<VkImageAspectFlags>(
                                   VK_IMAGE_ASPECT_DEPTH_BIT |
                                   (HasStencilComponent(depth_stencil_format)
                                        ? VK_IMAGE_ASPECT_STENCIL_BIT
                                        : 0U)),
                               0, 1, 0, 1},
      },
  };
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                       0, 0, nullptr, 0, nullptr, 2, attachment_barriers);

  const VkExtent2D pick_extent = {PICK_REGION_SIZE, PICK_REGION_SIZE};

  VkRenderingAttachmentInfo color_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = pick_color_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      // Slot 0 is the "nothing here" value the readback scan skips.
      .clearValue = {.color = {.uint32 = {0, 0, 0, 0}}},
  };

  VkRenderingAttachmentInfo depth_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .pNext = nullptr,
      .imageView = pick_depth_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .clearValue = {.depthStencil = {.depth = 1.0F, .stencil = 0}},
  };

  VkRenderingInfo rendering_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .pNext = nullptr,
      .renderArea = {.offset = {0, 0}, .extent = pick_extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
      .pDepthAttachment = &depth_attachment_info,
      .pStencilAttachment = HasStencilComponent(depth_stencil_format)
                                ? &depth_attachment_info
                                : nullptr,
  };

  vkCmdBeginRendering(command_buffer, &rendering_info);

  // The same negative-height flip the main pass uses. The engine derives the
  // pick projection assuming this convention, so the two must not drift apart.
  VkViewport viewport = {
      .x = 0.0F,
      .y = static_cast<f32>(PICK_REGION_SIZE),
      .width = static_cast<f32>(PICK_REGION_SIZE),
      .height = -static_cast<f32>(PICK_REGION_SIZE),
      .minDepth = 0.0F,
      .maxDepth = 1.0F,
  };
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor = {.offset = {0, 0}, .extent = pick_extent};
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->vk_pipeline);

  // No descriptor set is bound: the pick shaders read nothing from set 0, and
  // Vulkan only requires sets a shader statically uses.
  for (const auto &draw : draws) {
    const auto draw_item = draw_item_registry.Get(draw.draw_item_index);
    auto render_mesh_opt =
        render_mesh_manager.Get(draw_item.render_mesh_handle);
    if (!render_mesh_opt.has_value() || !render_mesh_opt.value()->ready) {
      continue;
    }
    const auto &render_mesh = render_mesh_opt.value();

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1,
                           &render_mesh->vertex_streams[0].buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, render_mesh->index_buffer, 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(command_buffer, pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(math::Mat4),
                       &draw.mvp);
    // firstInstance is the id channel: gl_InstanceIndex is firstInstance plus
    // the instance number, and there is exactly one instance.
    vkCmdDrawIndexed(command_buffer, static_cast<u32>(render_mesh->index_count),
                     1, 0, 0, draw.slot);
  }

  vkCmdEndRendering(command_buffer);

  VkImageMemoryBarrier readback_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = pick_color_image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
  };
  vkCmdPipelineBarrier(command_buffer,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &readback_barrier);

  VkBufferImageCopy region = {};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {PICK_REGION_SIZE, PICK_REGION_SIZE, 1};
  vkCmdCopyImageToBuffer(command_buffer, pick_color_image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         pick_readback_buffer, 1, &region);

  // The copy lands when this frame's submission completes, which is what its
  // fence signals. Registering it here rather than on the frame context is what
  // lets the context be handed back to the update thread before the result is
  // claimed.
  pending_pick_fence = frame_context.GetFrameBeginReadyFence();

  // No submission here: this is the frame's command buffer, so the copy lands
  // when the frame does and ProcessReadbacks publishes the answer.
}

auto LuminaRenderer::PublishPickFromReadback() -> void {
  const auto *pixels = static_cast<const u32 *>(pick_readback_buffer_mapped);
  constexpr auto centre = static_cast<i32>(PICK_REGION_SIZE) / 2;

  // Nearest hit to the centre wins, so a click a pixel or two off a thin object
  // still selects it rather than whatever sits behind the exact pixel.
  u32 best_slot = 0;
  i32 best_distance_squared = std::numeric_limits<i32>::max();
  for (i32 y = 0; y < static_cast<i32>(PICK_REGION_SIZE); ++y) {
    for (i32 x = 0; x < static_cast<i32>(PICK_REGION_SIZE); ++x) {
      const u32 slot = pixels[(y * static_cast<i32>(PICK_REGION_SIZE)) + x];
      if (slot == 0) {
        continue;
      }
      const i32 dx = x - centre;
      const i32 dy = y - centre;
      if (const i32 distance_squared = (dx * dx) + (dy * dy);
          distance_squared < best_distance_squared) {
        best_distance_squared = distance_squared;
        best_slot = slot;
      }
    }
  }

  PublishPickResult(best_slot);
}

auto LuminaRenderer::CreateRenderMesh(const core::StaticMesh &mesh,
                                      MaterialTemplateHandle material_template)
    -> RenderMeshHandle {
  GraphicsPipelineHandle pipeline_handle;
  const VertexBufferLayout *layout = nullptr;
  pipeline_manager.ForEach(
      [&](GraphicsPipelineHandle handle, GraphicsPipeline &pipeline) {
        if (pipeline.desc.material_template.index == material_template.index &&
            pipeline.desc.topology == mesh.topology) {
          pipeline_handle = handle;
          layout = &pipeline.desc.vertex_layout;
        }
      });
  LUMINA_CHECK(layout != nullptr,
               "No pipeline found matching material template and mesh "
               "topology");

  auto &cmd_ctx = AcquireCommandContext();
  cmd_ctx.Begin();
  RenderMesh render_mesh;
  render_mesh.pipeline_handle = pipeline_handle;
  auto vertex_streams_data = SerializeVertexBuffer(mesh, *layout);
  render_mesh.vertex_count = mesh.vertex_count;
  std::vector<VulkanBufferResources> staging_resources;
  for (const auto &[stream_stride, vertex_stream] : vertex_streams_data) {
    render_mesh.vertex_streams.emplace_back();
    auto [success, staging_buffer, device_buffer] =
        RecordVertexBufferUploadCommands(vulkan_context, cmd_ctx.command_buffer,
                                         vertex_stream.View());
    LUMINA_CHECK(success, "Failed to create vertex buffer");
    auto &render_mesh_vertex_stream = render_mesh.vertex_streams.back();
    render_mesh_vertex_stream.buffer = device_buffer.buffer;
    render_mesh_vertex_stream.memory = device_buffer.memory;
    render_mesh_vertex_stream.stride = stream_stride;
    staging_resources.emplace_back(staging_buffer);
  }

  DataBuffer index_buffer = DataBuffer(mesh.indices.size() * sizeof(u16));
  index_buffer.Write(0, mesh.indices.data(), mesh.indices.size() * sizeof(u16));
  auto [success, staging_buffer, device_buffer] =
      RecordIndexBufferUploadCommands(
          *cmd_ctx.vulkan_context, cmd_ctx.command_buffer, index_buffer.View());
  LUMINA_CHECK(success, "Failed to create index buffer");
  render_mesh.index_buffer = device_buffer.buffer;
  render_mesh.index_buffer_memory = device_buffer.memory;
  render_mesh.index_count = mesh.indices.size();
  staging_resources.emplace_back(staging_buffer);
  render_mesh.ready = false;
  auto render_mesh_handle = render_mesh_manager.Create(std::move(render_mesh));
  cmd_ctx.AddCompletionCallback(
      [staging = std::move(staging_resources), render_mesh_handle,
       mesh_manager = &this->render_mesh_manager](CommandContext *ctx) -> void {
        for (const auto &staging_resource : staging) {
          vkDestroyBuffer(ctx->vulkan_context->GetDevice(),
                          staging_resource.buffer, nullptr);
          vkFreeMemory(ctx->vulkan_context->GetDevice(),
                       staging_resource.memory, nullptr);
        }
        auto patch = [](RenderMesh &patched_mesh) -> void {
          patched_mesh.ready = true;
        };
        mesh_manager->Update(render_mesh_handle, patch);
      });
  cmd_ctx.End();
  SubmitCommandContext(cmd_ctx);
  return render_mesh_handle;
}

auto LuminaRenderer::DestroyRenderMesh(RenderMeshHandle handle) -> void {
  auto on_destroy =
      [context = &this->vulkan_context](RenderMeshHandle /*handle*/,
                                        const RenderMesh &mesh) -> void {
    for (const auto &stream : mesh.vertex_streams) {
      if (stream.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->GetDevice(), stream.buffer, nullptr);
      }
      if (stream.memory != VK_NULL_HANDLE) {
        vkFreeMemory(context->GetDevice(), stream.memory, nullptr);
      }
    }
    if (mesh.index_buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(context->GetDevice(), mesh.index_buffer, nullptr);
    }
    if (mesh.index_buffer_memory != VK_NULL_HANDLE) {
      vkFreeMemory(context->GetDevice(), mesh.index_buffer_memory, nullptr);
    }
  };
  render_mesh_manager.Destroy(handle, on_destroy);
}

auto LuminaRenderer::SubmitCommandContext(CommandContext &cmd_ctx) -> void {
  global_submission_queue.Push(&cmd_ctx);
}

auto LuminaRenderer::CreateRenderTexture(const core::Texture &texture)
    -> RenderTextureHandle {
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
  u32 bytes_per_pixel = 0;
  switch (texture.format) {
    case core::TextureFormat::R8_Unorm:
      vk_format = VK_FORMAT_R8_UNORM;
      bytes_per_pixel = 1;
      break;
    case core::TextureFormat::RGBA8_Srgb:
      vk_format = VK_FORMAT_R8G8B8A8_SRGB;
      bytes_per_pixel = 4;
      break;
  }

  const VkDeviceSize image_size = static_cast<VkDeviceSize>(texture.width) *
                                  texture.height * bytes_per_pixel;

  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
  CreateBuffer(vulkan_context, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer_memory, staging_buffer);
  void *mapped = nullptr;
  vkMapMemory(vulkan_context.GetDevice(), staging_buffer_memory, 0, image_size,
              0, &mapped);
  memcpy(mapped, texture.pixels.Data(), static_cast<size_t>(image_size));
  vkUnmapMemory(vulkan_context.GetDevice(), staging_buffer_memory);

  RenderTexture render_texture;
  auto image_created = CreateImage(
      vulkan_context, render_texture.image, texture.width, texture.height,
      render_texture.memory, vk_format, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  LUMINA_CHECK(image_created, "Failed to create render texture image");

  auto &cmd_ctx = AcquireCommandContext();
  cmd_ctx.Begin();

  // Transition UNDEFINED → TRANSFER_DST
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = render_texture.image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(
      cmd_ctx.command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  // Copy staging buffer → image
  VkBufferImageCopy region = {};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {texture.width, texture.height, 1};
  vkCmdCopyBufferToImage(cmd_ctx.command_buffer, staging_buffer,
                         render_texture.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // Transition TRANSFER_DST → SHADER_READ_ONLY
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd_ctx.command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  cmd_ctx.End();

  auto image_view_result = vulkan_context.CreateImageView(
      render_texture.image, vk_format, VK_IMAGE_ASPECT_COLOR_BIT);
  LUMINA_CHECK(image_view_result, "Failed to create render texture image view");
  render_texture.image_view = image_view_result.value();

  VkSamplerCreateInfo sampler_info = {};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  const auto address_mode = (texture.format == core::TextureFormat::R8_Unorm)
                                ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                                : VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeU = address_mode;
  sampler_info.addressModeV = address_mode;
  sampler_info.addressModeW = address_mode;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  vkCreateSampler(vulkan_context.GetDevice(), &sampler_info, nullptr,
                  &render_texture.sampler);

  auto rt_image_view = render_texture.image_view;
  auto rt_sampler = render_texture.sampler;
  const bool is_font_atlas = (vk_format == VK_FORMAT_R8_UNORM);
  auto handle = render_texture_manager.Create(std::move(render_texture));
  cmd_ctx.AddCompletionCallback(
      [staging_buffer, staging_buffer_memory, handle, is_font_atlas,
       texture_manager = &this->render_texture_manager, renderer = this,
       rt_image_view, rt_sampler](CommandContext *ctx) -> void {
        vkDestroyBuffer(ctx->vulkan_context->GetDevice(), staging_buffer,
                        nullptr);
        vkFreeMemory(ctx->vulkan_context->GetDevice(), staging_buffer_memory,
                     nullptr);
        texture_manager->Update(
            handle, [](RenderTexture &rt) -> void { rt.ready = true; });
        UpdateLitMaterialTextures(renderer, rt_sampler, rt_image_view);
        if (is_font_atlas) {
          renderer->ui_renderer.SetFontAtlas(*ctx->vulkan_context,
                                             rt_image_view, rt_sampler);
        }
      });
  SubmitCommandContext(cmd_ctx);
  return handle;
}

auto LuminaRenderer::DestroyRenderTexture(RenderTextureHandle handle) -> void {
  render_texture_manager.Destroy(
      handle, [context = &this->vulkan_context](RenderTextureHandle /*handle*/,
                                                const RenderTexture &rt) {
        if (rt.sampler != VK_NULL_HANDLE) {
          vkDestroySampler(context->GetDevice(), rt.sampler, nullptr);
        }
        if (rt.image_view != VK_NULL_HANDLE) {
          vkDestroyImageView(context->GetDevice(), rt.image_view, nullptr);
        }
        if (rt.image != VK_NULL_HANDLE) {
          vkDestroyImage(context->GetDevice(), rt.image, nullptr);
        }
        if (rt.memory != VK_NULL_HANDLE) {
          vkFreeMemory(context->GetDevice(), rt.memory, nullptr);
        }
      });
}

// A ceiling on runaway growth rather than a scene limit: at 64 bytes per
// instance this is 16 MB per frame context, well above any visible set the
// current cull can produce.
static constexpr size_t MAX_INSTANCE_BUFFER_CAPACITY = 1U << 18U;

auto LuminaRenderer::EnsureInstanceBufferCapacity(
    FrameContextInstanceBuffer &instance_buffer, size_t capacity) -> void {
  if (instance_buffer.capacity >= capacity) {
    return;
  }
  LUMINA_CHECK(capacity <= MAX_INSTANCE_BUFFER_CAPACITY,
               "Instance buffer capacity exceeds maximum");

  // Geometric growth so a steadily rising visible count stops reallocating
  // after a few frames. Computed before the destroy, while capacity still holds
  // the old size.
  const auto new_capacity =
      math::Max(capacity, static_cast<size_t>(instance_buffer.capacity) * 2UZ);

  // Destroying here is safe only because a frame context is handed out for
  // update after TryReclaimFrameContexts saw its fence signalled, so the GPU
  // has finished reading the old buffer.
  vkUnmapMemory(vulkan_context.GetDevice(), instance_buffer.memory);
  vkDestroyBuffer(vulkan_context.GetDevice(), instance_buffer.buffer, nullptr);
  vkFreeMemory(vulkan_context.GetDevice(), instance_buffer.memory, nullptr);

  LUMINA_CHECK(CreateInstanceBuffer(vulkan_context, instance_buffer,
                                    static_cast<u32>(new_capacity)),
               "Failed to create instance buffer");
}

auto LuminaRenderer::GetRenderTexture(RenderTextureHandle handle) noexcept
    -> std::optional<const RenderTexture *> {
  return render_texture_manager.Get(handle);
}

auto LuminaRenderer::GetRenderMesh(RenderMeshHandle handle) noexcept
    -> std::optional<const RenderMesh *> {
  return render_mesh_manager.Get(handle);
}

} // namespace lumina::renderer
