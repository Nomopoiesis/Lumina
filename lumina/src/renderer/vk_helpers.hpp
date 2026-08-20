#pragma once

#include "lumina_assert.hpp"

#include "primitive_topology.hpp"
#include "shaders/shader_layout.hpp"


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

inline auto ToVkFormat(core::ElementType element_type) -> VkFormat {
  switch (element_type) {
    case core::ElementType::Float:
      return VK_FORMAT_R32_SFLOAT;
    case core::ElementType::Vec2:
      return VK_FORMAT_R32G32_SFLOAT;
    case core::ElementType::Vec3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case core::ElementType::Vec4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case core::ElementType::Uint32:
      return VK_FORMAT_R32_UINT;
    // No vertex stream uses these yet. They are listed rather than folded into
    // a `default:` so that adding an ElementType breaks this switch instead of
    // reaching the assert below at runtime.
    case core::ElementType::Double:
    case core::ElementType::Int8:
    case core::ElementType::Uint8:
    case core::ElementType::Int16:
    case core::ElementType::Uint16:
    case core::ElementType::Int32:
    case core::ElementType::Int64:
    case core::ElementType::Uint64:
    case core::ElementType::Bool:
      break;
  }
  ASSERT(false, "Unsupported element type for Vulkan vertex format");
  return VK_FORMAT_UNDEFINED;
}

inline auto HasStencilComponent(VkFormat format) -> bool {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace lumina::renderer
