#pragma once

#include "resource_manager_handle.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

struct RenderTexture {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView image_view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  bool ready = false;
};

using RenderTextureHandle = core::ResourceHandle<RenderTexture>;

} // namespace lumina::renderer
