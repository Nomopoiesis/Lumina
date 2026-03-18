#include "static_shader_api.hpp"

#include "headers/interface.global.hpp"
#include "headers/standard_lit.frag.hpp"
#include "headers/standard_lit.vert.hpp"

#include "shaders/shader_vk_helpers.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::renderer {

auto BuildStaticMaterialTemplates(LuminaRenderer *renderer) -> void {
  auto shader_interface_result = ShaderInterface::Create(
      renderer->GetVulkanContext().GetDevice(),
      lumina::shaders::standard_lit::vert::kLayout,
      lumina::shaders::standard_lit::frag::kLayout, "standard_lit",
      renderer->GetGlobalDescriptorSetLayout(),
      lumina::shaders::interface::global::kLayout);
  if (!shader_interface_result) {
    LOG_CRITICAL("Failed to create shader interface: {}",
                 shader_interface_result.error().message);
    LUMINA_TERMINATE();
  }

  auto shader_interface_index =
      renderer->AddShaderInterface(std::move(shader_interface_result.value()));

  MaterialTemplateDescription mat_desc = {
      .shader_interface_index = shader_interface_index,
      .vertex_layout = lumina::shaders::standard_lit::vert::kLayout,
      .fragment_layout = lumina::shaders::standard_lit::frag::kLayout,
      .vertex_shader_bin_path = "shaders/spv/shader.vert.spv",
      .fragment_shader_bin_path = "shaders/spv/shader.frag.spv",
      .max_instances = 2,
  };
  auto mat_template_handle = renderer->CreateMaterialTemplate(mat_desc);

  renderer->SetDefaultMaterialTemplate(mat_template_handle);
}

auto CreateGlobalDescriptorSetLayout(LuminaRenderer *renderer)
    -> std::expected<void, VkInitializationError> {
  namespace g = lumina::shaders::interface::global;

  auto *device = renderer->vulkan_context.GetDevice();

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(g::kLayout.binding_count);
  for (u32 i = 0; i < g::kLayout.binding_count; ++i) {
    const auto &b = g::kBindings[i];
    bindings.push_back({
        .binding = b.binding,
        .descriptorType = ToVkDescriptorType(b.type),
        .descriptorCount = b.array_count,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
    });
  }

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = static_cast<u32>(bindings.size()),
      .pBindings = bindings.data(),
  };

  if (auto result =
          vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
                                      &renderer->global_descriptor_set_layout);
      result != VK_SUCCESS) {
    return std::unexpected(VkInitializationError{
        .message = "Failed to create global descriptor set layout: " +
                   std::string(string_VkResult(result))});
  }

  // Create a pipeline layout with only set 0, used for binding the global
  // descriptor set. Push constant ranges must match those of the full pipeline
  // layouts for Vulkan compatibility.
  std::vector<VkPushConstantRange> push_constant_ranges;
  if (g::kLayout.push_constant_size > 0) {
    push_constant_ranges.push_back({
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = g::kLayout.push_constant_offset,
        .size = g::kLayout.push_constant_size,
    });
  }

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = 1,
      .pSetLayouts = &renderer->global_descriptor_set_layout,
      .pushConstantRangeCount = static_cast<u32>(push_constant_ranges.size()),
      .pPushConstantRanges = push_constant_ranges.data(),
  };

  if (auto result =
          vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                                 &renderer->global_pipeline_layout);
      result != VK_SUCCESS) {
    return std::unexpected(VkInitializationError{
        .message = "Failed to create global pipeline layout: " +
                   std::string(string_VkResult(result))});
  }

  return {};
}

auto GetGlobalDescriptorPoolSizes(std::vector<VkDescriptorPoolSize> &pool_sizes)
    -> void {
  namespace g = lumina::shaders::interface::global;
  pool_sizes.reserve(g::kLayout.binding_count);
  for (const auto &kBinding : g::kBindings) {
    pool_sizes.push_back({
        .type = ToVkDescriptorType(kBinding.type),
        .descriptorCount = kBinding.array_count,
    });
  }
}

} // namespace lumina::renderer