#pragma once

#include "frame_context.hpp"
#include "material_template_handle.hpp"

#include <array>
#include <vulkan/vulkan.h>

namespace lumina::renderer {

struct MaterialInstanceCreateError {
  std::string message;
};

class MaterialInstance {
public: // static methods
  static auto Create(MaterialTemplateHandle tmpl_handle) noexcept
      -> std::expected<MaterialInstance, MaterialInstanceCreateError>;

public: // public methods
  MaterialInstance() noexcept = default;
  MaterialInstance(const MaterialInstance &) noexcept = default;
  MaterialInstance(MaterialInstance &&) noexcept = default;
  auto operator=(const MaterialInstance &) noexcept
      -> MaterialInstance & = default;
  auto operator=(MaterialInstance &&) noexcept -> MaterialInstance & = default;
  ~MaterialInstance() noexcept = default;

  [[nodiscard]] auto GetTemplateHandle() const noexcept
      -> MaterialTemplateHandle {
    return material_template_handle;
  }

  [[nodiscard]] auto GetDescriptorSet(size_t frame_index) const noexcept
      -> const VkDescriptorSet & {
    return descriptor_sets[frame_index];
  }

  [[nodiscard]] auto GetDescriptorSets() const noexcept
      -> const std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> & {
    return descriptor_sets;
  }

  [[nodiscard]] auto GetDescriptorSetsMutable() noexcept
      -> std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> & {
    return descriptor_sets;
  }

  auto SetTemplateHandle(MaterialTemplateHandle handle) noexcept -> void {
    material_template_handle = handle;
  }

  auto ResetDescriptorSets() noexcept -> void {
    descriptor_sets.fill(VK_NULL_HANDLE);
  }

private: // private members
  MaterialTemplateHandle material_template_handle;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptor_sets{};
};

} // namespace lumina::renderer