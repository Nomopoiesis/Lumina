#include "renderer.hpp"

#include "common/data_structures/data_buffer.hpp"
#include "common/logger/logger.hpp"
#include "common/lumina_assert.hpp"
#include "common/lumina_terminate.hpp"
#include "core/lumina_engine.hpp"
#include "graphics_pipeline.hpp"
#include "shaders/shader_module_cache.hpp"
#include "vertex_layout.hpp"
#include "vertex_serializer.hpp"

#include "math/vector.hpp"
#include <vulkan/vk_enum_string_helper.h>

#include <array>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

namespace lumina::renderer {

using namespace lumina::common::data_structures;

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

  VkDescriptorSetLayoutBinding sampler_layout_binding = {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
  };

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
      ubo_layout_binding, sampler_layout_binding};

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = static_cast<u32>(bindings.size()),
      .pBindings = bindings.data(),
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

static auto HasStencilComponent(VkFormat format) -> bool {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
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
  ASSERT(copy_result, "Failed to copy buffer");

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
  ASSERT(copy_result, "Failed to copy buffer");

  return std::make_tuple(true, staging_buffer, device_buffer);
}

static auto CreateDescriptorPool(VulkanContext &vulkan_context,
                                 VkDescriptorPool &descriptor_pool,
                                 size_t descriptor_count) -> bool {
  std::array<VkDescriptorPoolSize, 2> pool_sizes{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = descriptor_count;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = descriptor_count;

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .maxSets = SafeU64ToU32(descriptor_count),
      .poolSizeCount = static_cast<u32>(pool_sizes.size()),
      .pPoolSizes = pool_sizes.data(),
  };
  if (auto result = vkCreateDescriptorPool(
          vulkan_context.GetDevice(), &pool_info, nullptr, &descriptor_pool);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to create descriptor pool: {}", string_VkResult(result));
    return false;
  }
  return true;
}

static auto
CreateDescriptorSets(VulkanContext &vulkan_context,
                     VkDescriptorPool &descriptor_pool, size_t descriptor_count,
                     VkBuffer *uniform_buffers, VkSampler &texture_sampler,
                     VkImageView &texture_image_view,
                     VkDescriptorSetLayout &descriptor_set_layout,
                     std::vector<VkDescriptorSet> &descriptor_sets) -> bool {
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
        .range = sizeof(core::UniformBufferObject),
    };
    VkDescriptorImageInfo image_info = {
        .sampler = texture_sampler,
        .imageView = texture_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    std::array<VkWriteDescriptorSet, 2> write_descriptor_sets = {
        VkWriteDescriptorSet{
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
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptor_sets[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        },
    };
    vkUpdateDescriptorSets(vulkan_context.GetDevice(),
                           static_cast<u32>(write_descriptor_sets.size()),
                           write_descriptor_sets.data(), 0, nullptr);
  }
  return true;
}

static auto CreateUniformBuffer(VulkanContext &vulkan_context,
                                VkDeviceMemory &memory, VkBuffer &buffer,
                                void *&mapped_data) -> bool {
  VkDeviceSize size = sizeof(core::UniformBufferObject);
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
      stbi_load("C:/Projects/c++/Lumina/lumina/data/textures/tex.png",
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
  if (!CreateImage(vulkan_context, texture_image, tex_width, tex_height,
                   texture_image_memory, format, VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
    LOG_ERROR("Failed to create texture image");
    return false;
  }

  TransitionImageLayout(vulkan_context, command_pool, texture_image, format,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CopyBufferToImage(vulkan_context, command_pool, staging_buffer, texture_image,
                    tex_width, tex_height);
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

static auto CreateDepthResources(VulkanContext &vulkan_context,
                                 VkCommandPool &command_pool,
                                 VkImage &depth_image,
                                 VkImageView &depth_image_view,
                                 VkDeviceMemory &depth_image_memory,
                                 VkFormat &depth_stencil_format) -> bool {
  auto depth_format = FindDepthFormat(vulkan_context);
  ASSERT(depth_format, "Failed to find depth format");
  depth_stencil_format = depth_format.value();
  auto width = vulkan_context.GetSwapChainImageExtent().width;
  auto height = vulkan_context.GetSwapChainImageExtent().height;
  CreateImage(vulkan_context, depth_image, width, height, depth_image_memory,
              depth_stencil_format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  auto depth_image_view_result = vulkan_context.CreateImageView(
      depth_image, depth_format.value(), VK_IMAGE_ASPECT_DEPTH_BIT);
  ASSERT(depth_image_view_result, "Failed to create depth image view");
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

  render_mesh_manager.DestroyAll(
      [this](RenderMeshHandle handle) -> void { DestroyRenderMesh(handle); });

  if (depth_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan_context.GetDevice(), depth_image_view, nullptr);
  }
  if (depth_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan_context.GetDevice(), depth_image, nullptr);
  }
  if (depth_image_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), depth_image_memory, nullptr);
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

  if (descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vulkan_context.GetDevice(), descriptor_pool,
                            nullptr);
  }

  if (descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan_context.GetDevice(),
                                 descriptor_set_layout, nullptr);
  }

  if (command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(vulkan_context.GetDevice(), command_pool, nullptr);
  }
  pipeline_manager.DestroyAll([this](GraphicsPipelineHandle handle) {
    auto pipeline_opt = pipeline_manager.Get(handle);
    if (pipeline_opt.has_value() &&
        pipeline_opt->vk_pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(vulkan_context.GetDevice(), pipeline_opt->vk_pipeline,
                        nullptr);
    }
  });

  if (pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(vulkan_context.GetDevice(), pipeline_layout,
                            nullptr);
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
  ASSERT(frame_context_for_update, "No frame context for update");
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
  ASSERT(frame_context_for_render, "No frame context for render");
  frame_context_for_render->pipeline_state.store(
      FrameContextPipelineState::RENDER_COMPLETE);
  frame_context_for_render = nullptr;
  // We do not signal that a frame context is available for update, as it may
  // not yet be done rendering on the GPU (due to it being async), instead we
  // will signal the availability when a frame context is reclaimed
}

auto LuminaRenderer::TryReclaimFrameContexts() -> void {
  // First check fence status of all RENDER_COMPLETE frame contexts
  auto pending_completion = 0;
  for (auto &frame_context : frame_contexts) {
    if (frame_context->pipeline_state.load() ==
        FrameContextPipelineState::RENDER_COMPLETE) {
      auto status = vkGetFenceStatus(vulkan_context.GetDevice(),
                                     frame_context->GetFrameBeginReadyFence());
      if (status == VK_SUCCESS) {
        // The fence is signaled, so the frame context is ready to be reused
        frame_context->pipeline_state.store(FrameContextPipelineState::IDLE);
        frames_available_for_update.release();
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
    ASSERT(ctx_idx < frame_contexts.size(), "Context index out of bounds");
    vkWaitForFences(vulkan_context.GetDevice(), 1,
                    &frame_contexts[ctx_idx]->GetFrameBeginReadyFence(),
                    VK_TRUE, UINT64_MAX);
    frame_contexts[ctx_idx]->pipeline_state.store(
        FrameContextPipelineState::IDLE);
    frames_available_for_update.release();
  }
}

auto LuminaRenderer::RenderThread() -> void {
  platform::common::PlatformServices::Instance().LuminaSetThreadName(
      "LuminaRendererThread");
  while (!shutdown_requested) {
    PollAndExecuteCommandContexts();
    AcquireFrameContextForRender();
    DrawFrame();
    ReleaseFrameContextForRender();
    TryReclaimFrameContexts();
  }
}

auto LuminaRenderer::PollAndExecuteCommandContexts() -> void {
  auto size = global_submission_queue.Size();
  std::vector<CommandContext *> cmd_ctxs(size);
  for (size_t i = 0; i < size; ++i) {
    auto res = global_submission_queue.Pop(cmd_ctxs[i]);
    ASSERT(res, "Failed to pop command context from submission queue");
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
    auto &[fence, cmd_ctxs] = *it;
    auto status = vkGetFenceStatus(vulkan_context.GetDevice(), fence);
    if (status == VK_SUCCESS) {
      vulkan_context.DestroyFence(fence);
      for (auto &cmd_ctx : cmd_ctxs) {
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
  if (!vulkan_context.Initialize()) {
    LOG_CRITICAL("Failed to initialize Vulkan context: {}",
                 vulkan_context.Initialize().error().message);
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

  auto desc_result =
      CreateDescriptorSetLayout(vulkan_context, descriptor_set_layout);
  ASSERT(desc_result, "Failed to create descriptor set layout");

  if (!CreatePipelineLayout()) {
    LOG_CRITICAL("Failed to create pipeline layout: {}",
                 CreatePipelineLayout().error().message);
    LUMINA_TERMINATE();
  }

  auto depth_result = CreateDepthResources(
      vulkan_context, command_pool, depth_image, depth_image_view,
      depth_image_memory, depth_stencil_format);
  ASSERT(depth_result, "Failed to create depth resources");

  auto res = CreateTextureImage(vulkan_context, command_pool, texture_image,
                                texture_image_memory);
  ASSERT(res, "Failed to create texture image");
  auto texture_image_view_result =
      CreateTextureImageView(vulkan_context, texture_image);
  ASSERT(texture_image_view_result, "Failed to create texture image view");
  texture_image_view = texture_image_view_result.value();

  auto texture_sampler_result =
      CreateTextureSampler(vulkan_context, texture_sampler);
  ASSERT(texture_sampler_result, "Failed to create texture sampler");
  texture_sampler = texture_sampler_result.value();

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
    auto uniform_buffer_result =
        CreateUniformBuffer(vulkan_context, uniform_buffer.memory,
                            uniform_buffer.buffer, uniform_buffer.mapped);
    ASSERT(uniform_buffer_result, "Failed to create uniform buffer");
  }

  auto desc_pool_result = CreateDescriptorPool(vulkan_context, descriptor_pool,
                                               MAX_FRAMES_IN_FLIGHT);
  ASSERT(desc_pool_result, "Failed to create descriptor pool");
  std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniform_buffers{};
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    uniform_buffers[i] = frame_contexts[i]->GetUniformBuffer().buffer;
  }
  auto desc_sets_result = CreateDescriptorSets(
      vulkan_context, descriptor_pool, MAX_FRAMES_IN_FLIGHT,
      uniform_buffers.data(), texture_sampler, texture_image_view,
      descriptor_set_layout, descriptor_sets);
  ASSERT(desc_sets_result, "Failed to create descriptor sets");

  render_thread = std::thread(&LuminaRenderer::RenderThread, this);
}

auto LuminaRenderer::Shutdown() -> void {
  shutdown_requested = true;
  render_thread.join();
}

auto LuminaRenderer::DeviceWaitIdle() -> void {
  vkDeviceWaitIdle(vulkan_context.GetDevice());
}

auto LuminaRenderer::DrawFrame() -> void {
  auto &frame_context = *frame_context_for_render;

  vkResetFences(vulkan_context.GetDevice(), 1,
                &frame_context.GetFrameBeginReadyFence());

  u32 image_index = 0;
  VkResult result = vkAcquireNextImageKHR(
      vulkan_context.GetDevice(), vulkan_context.GetSwapChain(), UINT64_MAX,
      frame_context.GetFrameBeginSemaphore(), VK_NULL_HANDLE, &image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
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

  if (auto record_command_buffer_result =
          RecordCommandBuffer(frame_context, image_index);
      !record_command_buffer_result) {
    LOG_CRITICAL("Failed to record command buffer: {}",
                 record_command_buffer_result.error().message);
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

auto LuminaRenderer::CreatePipelineLayout()
    -> std::expected<void, VkInitializationError> {
  VkPushConstantRange range = {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = sizeof(math::Mat4),
  };

  VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &range,
  };

  if (vkCreatePipelineLayout(vulkan_context.GetDevice(),
                             &pipeline_layout_create_info, nullptr,
                             &pipeline_layout) != VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create pipeline layout"});
  }

  return std::expected<void, VkInitializationError>{};
}

static auto ToVkFormat(core::ElementType element_type) -> VkFormat {
  switch (element_type) {
    case core::ElementType::Float:
      return VK_FORMAT_R32_SFLOAT;
    case core::ElementType::Vec2:
      return VK_FORMAT_R32G32_SFLOAT;
    case core::ElementType::Vec3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case core::ElementType::Vec4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      ASSERT(false, "Unsupported element type for Vulkan vertex format");
      return VK_FORMAT_UNDEFINED;
  }
}

static auto ToVkBindingDescriptions(const VertexBufferLayout &layout)
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

static auto ToVkAttributeDescriptions(const VertexBufferLayout &layout)
    -> std::vector<VkVertexInputAttributeDescription> {
  std::vector<VkVertexInputAttributeDescription> descriptions;
  u32 location = 0;
  for (u32 binding = 0; binding < static_cast<u32>(layout.streams.size());
       ++binding) {
    u32 offset = 0;
    for (const auto &attr : layout.streams[binding].attributes) {
      descriptions.push_back({
          .location = location++,
          .binding = binding,
          .format = ToVkFormat(attr.element_type),
          .offset = offset,
      });
      offset += core::GetElementTypeSize(attr.element_type);
    }
  }
  return descriptions;
}

auto LuminaRenderer::CreateGraphicsPipeline(const GraphicsPipelineDesc &desc)
    -> GraphicsPipelineHandle {
  ShaderModuleCache shader_module_cache(vulkan_context.GetDevice());

  auto vert_module_result = shader_module_cache.GetShaderModule(
      "C:/Projects/c++/Lumina/lumina/data/shaders/spv/shader.vert.spv",
      VK_SHADER_STAGE_VERTEX_BIT);
  ASSERT(vert_module_result, "Failed to create vertex shader");

  auto frag_module_result = shader_module_cache.GetShaderModule(
      "C:/Projects/c++/Lumina/lumina/data/shaders/spv/shader.frag.spv",
      VK_SHADER_STAGE_FRAGMENT_BIT);
  ASSERT(frag_module_result, "Failed to create fragment shader");

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
  auto attribute_descriptions = ToVkAttributeDescriptions(desc.vertex_layout);

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
  pipeline_dynamic_rendering_info.depthAttachmentFormat = depth_stencil_format;
  pipeline_dynamic_rendering_info.stencilAttachmentFormat =
      HasStencilComponent(depth_stencil_format) ? depth_stencil_format
                                                : VK_FORMAT_UNDEFINED;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{};
  depth_stencil_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
  depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
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
      .layout = pipeline_layout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  GraphicsPipeline gfx_pipeline;
  gfx_pipeline.vertex_layout = desc.vertex_layout;

  if (vkCreateGraphicsPipelines(vulkan_context.GetDevice(), nullptr, 1,
                                &pipeline_create_info, nullptr,
                                &gfx_pipeline.vk_pipeline) != VK_SUCCESS) {
    LOG_CRITICAL("Failed to create graphics pipeline");
    LUMINA_TERMINATE();
  }

  auto handle = pipeline_manager.Create();
  pipeline_manager.Set(handle, std::move(gfx_pipeline));
  return handle;
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

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout, 0, 1,
                          &descriptor_sets[current_frame_index], 0, nullptr);
  for (const auto &draw_mesh_info : frame_context.render_draw_list) {
    auto handle = draw_mesh_info.render_mesh_handle;
    auto model = draw_mesh_info.model;
    auto render_mesh_opt = render_mesh_manager.Get(handle);
    if (!render_mesh_opt.has_value() || !render_mesh_opt.value().ready) {
      continue;
    }
    const auto &render_mesh = render_mesh_opt.value();
    auto pipeline_opt = pipeline_manager.Get(render_mesh.pipeline_handle);
    if (!pipeline_opt.has_value()) {
      continue;
    }
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline_opt->vk_pipeline);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1,
                           &render_mesh.vertex_streams[0].buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, render_mesh.index_buffer, 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(command_buffer, pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(math::Mat4),
                       &model);
    vkCmdDrawIndexed(command_buffer, static_cast<u32>(render_mesh.index_count),
                     1, 0, 0, 0);
  }

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

auto LuminaRenderer::AcquireCommandContext() -> CommandContext & {
  return *command_context_pool.Acquire();
}

auto LuminaRenderer::ReleaseCommandContext(CommandContext &cmd_ctx) -> void {
  command_context_pool.Release(&cmd_ctx);
}

auto LuminaRenderer::CreateRenderMesh(const core::StaticMesh &mesh,
                                      GraphicsPipelineHandle pipeline_handle)
    -> RenderMeshHandle {
  auto pipeline_opt = pipeline_manager.Get(pipeline_handle);
  ASSERT(pipeline_opt.has_value(),
         "Pipeline not found for render mesh creation");
  const VertexBufferLayout &layout = pipeline_opt->vertex_layout;

  auto &cmd_ctx = AcquireCommandContext();
  cmd_ctx.Begin();
  auto render_mesh_handle = render_mesh_manager.Create();
  auto render_mesh_opt = render_mesh_manager.Get(render_mesh_handle);
  ASSERT(render_mesh_opt.has_value(), "Render mesh not found");
  auto &render_mesh = render_mesh_opt.value();
  render_mesh.pipeline_handle = pipeline_handle;
  auto vertex_streams_data = SerializeVertexBuffer(mesh, layout);
  render_mesh.vertex_count = mesh.vertex_count;
  std::vector<VulkanBufferResources> staging_resources;
  for (const auto &[stream_stride, vertex_stream] : vertex_streams_data) {
    render_mesh.vertex_streams.emplace_back();
    auto [success, staging_buffer, device_buffer] =
        RecordVertexBufferUploadCommands(vulkan_context, cmd_ctx.command_buffer,
                                         vertex_stream.View());
    // TODO: Handle error gracefully
    ASSERT(success, "Failed to create vertex buffer");
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
  // TODO: Handle error gracefully
  ASSERT(success, "Failed to create index buffer");
  render_mesh.index_buffer = device_buffer.buffer;
  render_mesh.index_buffer_memory = device_buffer.memory;
  render_mesh.index_count = mesh.indices.size();
  staging_resources.emplace_back(staging_buffer);
  render_mesh.ready = false;
  render_mesh_manager.Set(render_mesh_handle, std::move(render_mesh));
  cmd_ctx.AddCompletionCallback(
      [staging = std::move(staging_resources), render_mesh_handle,
       render_mesh_manager =
           &this->render_mesh_manager](CommandContext *cmd_ctx) -> void {
        for (const auto &staging_buffer : staging) {
          vkDestroyBuffer(cmd_ctx->vulkan_context->GetDevice(),
                          staging_buffer.buffer, nullptr);
          vkFreeMemory(cmd_ctx->vulkan_context->GetDevice(),
                       staging_buffer.memory, nullptr);
        }
        auto mesh = render_mesh_manager->Get(render_mesh_handle);
        mesh->ready = true;
        render_mesh_manager->Set(render_mesh_handle, std::move(*mesh));
      });
  cmd_ctx.End();
  SubmitCommandContext(cmd_ctx);
  return render_mesh_handle;
}

auto LuminaRenderer::DestroyRenderMesh(RenderMeshHandle handle) -> void {
  auto render_mesh_opt = render_mesh_manager.Get(handle);
  if (!render_mesh_opt.has_value()) {
    return;
  }
  const auto &render_mesh = render_mesh_opt.value();
  for (const auto &stream : render_mesh.vertex_streams) {
    if (stream.buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(vulkan_context.GetDevice(), stream.buffer, nullptr);
    }
    if (stream.memory != VK_NULL_HANDLE) {
      vkFreeMemory(vulkan_context.GetDevice(), stream.memory, nullptr);
    }
  }
  if (render_mesh.index_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(vulkan_context.GetDevice(), render_mesh.index_buffer,
                    nullptr);
  }
  if (render_mesh.index_buffer_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan_context.GetDevice(), render_mesh.index_buffer_memory,
                 nullptr);
  }
  render_mesh_manager.Destroy(handle);
}

auto LuminaRenderer::SubmitCommandContext(CommandContext &cmd_ctx) -> void {
  global_submission_queue.Push(&cmd_ctx);
}
} // namespace lumina::renderer
