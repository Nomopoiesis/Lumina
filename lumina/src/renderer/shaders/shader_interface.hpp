#pragma once

#include "shader_layout.hpp"

#include "common/logger/logger.hpp"

#include <vulkan/vulkan.h>

#include <expected>
#include <string>
#include <vector>

namespace lumina::renderer {

struct ShaderInterfaceCreateError {
  std::string message;
};

class ShaderInterface {
public: // static methods
  static auto Create(VkDevice device, const ShaderLayout &vertex_layout,
                     const ShaderLayout &fragment_layout)
      -> std::expected<ShaderInterface, ShaderInterfaceCreateError>;

public: // instance methods
  ShaderInterface() = default;
  ShaderInterface(const ShaderInterface &) = delete;
  auto operator=(const ShaderInterface &) -> ShaderInterface & = delete;
  ShaderInterface(ShaderInterface &&) noexcept;
  auto operator=(ShaderInterface &&) noexcept -> ShaderInterface &;
  ~ShaderInterface() noexcept;

  // set_indices[sparse_set] = dense_index into descriptor_set_layouts.
  // UINT32_MAX means the set is not present.
  [[nodiscard]] auto GetDescriptorSetLayout(u32 set) const
      -> VkDescriptorSetLayout {
    if (set >= set_indices.size() || set_indices[set] == UINT32_MAX) {
      LOG_WARNING("Set index out of range: {}", set);
      return VK_NULL_HANDLE;
    }
    return descriptor_set_layouts[set_indices[set]];
  }
  [[nodiscard]] auto GetPipelineLayout() const -> VkPipelineLayout {
    return pipeline_layout;
  }

private: // instance members
  VkDevice m_device = VK_NULL_HANDLE;
  std::vector<u32>
      set_indices; // corresponds to the set index in the descriptor set layouts
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
};

} // namespace lumina::renderer