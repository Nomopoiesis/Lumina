#pragma once

#include "lumina_assert.hpp"

#include "shader_layout.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

static auto ToVkDescriptorType(DescriptorBindingType type) -> VkDescriptorType {
  switch (type) {
    case DescriptorBindingType::UniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorBindingType::Sampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorBindingType::CombinedImageSampler:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorBindingType::SampledImage:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorBindingType::StorageImage:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorBindingType::UniformTexelBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    case DescriptorBindingType::StorageTexelBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    case DescriptorBindingType::StorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    default:
      ASSERT(false, "Unsupported binding type");
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
}

} // namespace lumina::renderer