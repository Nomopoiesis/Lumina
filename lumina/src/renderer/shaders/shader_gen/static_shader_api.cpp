#include "static_shader_api.hpp"

#include "headers/interface.global.hpp"
#include "headers/simple_input_basic_mat.frag.hpp"
#include "headers/simple_model_input.vert.hpp"

#include "shaders/shader_vk_helpers.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::renderer {

auto BuildStaticMaterialTemplates(LuminaRenderer *renderer) -> void {
  namespace vert = lumina::shaders::simple_model_input::vert;
  namespace frag = lumina::shaders::simple_input_basic_mat::frag;
  namespace global = lumina::shaders::interface::global;

  auto shader_interface_result = ShaderInterface::Create(
      renderer->GetVulkanContext().GetDevice(), vert::kLayout, frag::kLayout,
      "standard_lit", renderer->GetGlobalDescriptorSetLayout(),
      global::kLayout);
  if (!shader_interface_result) {
    LOG_CRITICAL("Failed to create shader interface: {}",
                 shader_interface_result.error().message);
    LUMINA_TERMINATE();
  }

  auto shader_interface_index =
      renderer->AddShaderInterface(std::move(shader_interface_result.value()));

  MaterialTemplateDescription mat_desc = {
      .shader_interface_index = shader_interface_index,
      .vertex_layout = vert::kLayout,
      .fragment_layout = frag::kLayout,
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

auto GetDefaultMaterialUBOSize() -> VkDeviceSize {
  return sizeof(shaders::simple_input_basic_mat::frag::MaterialUniforms);
}

auto InitDefaultMaterialUBO(void *mapped_data) -> void {
  using MU = shaders::simple_input_basic_mat::frag::MaterialUniforms;
  auto *mu = static_cast<MU *>(mapped_data);
  mu->ambient_intensity = 0.05F;
  mu->ambient_color = {1.0F, 1.0F, 1.0F};
  mu->diffuse_color = {0.5F, 0.5F, 0.5F};
}

auto WriteDefaultMaterialDescriptors(LuminaRenderer *renderer) -> void {
  namespace mat = shaders::simple_input_basic_mat::frag;

  auto *instance = renderer->material_instance_manager.GetRef(
      renderer->default_material_instance_handle);
  ASSERT(instance != nullptr, "Default material instance not found");

  mat::BindingData bindings{
      .texSampler_sampler = renderer->texture_sampler,
      .texSampler_imageView = renderer->texture_image_view,
      .material_uniforms_buffer = renderer->default_material_ubo_buffer,
  };

  auto *device = renderer->vulkan_context.GetDevice();
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    mat::WriteDescriptors(device, instance->GetDescriptorSet(i), bindings);
  }
}

auto WriteTransientDescriptors(LuminaRenderer *renderer,
                               FrameContext &frame_context,
                               VkDescriptorSet descriptor_set) -> void {
  namespace global = shaders::interface::global;
  global::WriteDescriptors(
      renderer->vulkan_context.GetDevice(), descriptor_set,
      {.ubo_buffer = frame_context.GetUniformBuffer().buffer});
}

} // namespace lumina::renderer