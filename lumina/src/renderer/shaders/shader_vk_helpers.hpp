#pragma once

#include "lumina_assert.hpp"

#include "graphics_pipeline.hpp"
#include "shader_layout.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

inline auto ToVkPrimitiveTopology(PrimitiveTopology topology)
    -> VkPrimitiveTopology {
  switch (topology) {
    case PrimitiveTopology::TriangleList:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::LineList:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  }
  // See ToVkDescriptorType below: the fallback lives outside the switch so
  // -Wswitch flags a newly added topology at compile time.
  ASSERT(false, "Unsupported primitive topology");
  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

inline auto ToVkDescriptorType(DescriptorBindingType type) -> VkDescriptorType {
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
  }
  // A `default:` here would cover every enumerator and so disable -Wswitch,
  // turning "somebody added a binding type" from a build break into a runtime
  // assert. Keeping the fallback after the switch preserves both.
  ASSERT(false, "Unsupported binding type");
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

} // namespace lumina::renderer
