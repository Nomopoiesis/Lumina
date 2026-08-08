#include "static_shader_api.hpp"

#include "common/lumina_check.hpp"
#include "common/path_registry.hpp"
#include "headers/color_output.frag.hpp"
#include "headers/interface.global.hpp"
#include "headers/position_only.vert.hpp"
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
      .vertex_shader_bin_path = lumina::common::PathRegistry::Instance()
                                    .shaders.Resolve("shader.vert.spv")
                                    .string(),
      .fragment_shader_bin_path = lumina::common::PathRegistry::Instance()
                                      .shaders.Resolve("shader.frag.spv")
                                      .string(),
      // Sizes both the persistent descriptor pool and the material uniform
      // buffer, so it has to cover the default instance plus every coloured
      // instance the scene creates.
      .max_instances = 16,
  };
  auto mat_template_handle = renderer->CreateMaterialTemplate(mat_desc);

  renderer->SetDefaultMaterialTemplate(mat_template_handle);

  // Debug wireframe — position-only, no set 1 bindings, no material instances
  namespace dbg_vert = lumina::shaders::position_only::vert;
  namespace dbg_frag = lumina::shaders::color_output::frag;

  auto dbg_interface_result = ShaderInterface::Create(
      renderer->GetVulkanContext().GetDevice(), dbg_vert::kLayout,
      dbg_frag::kLayout, "debug_wireframe",
      renderer->GetGlobalDescriptorSetLayout(), global::kLayout);
  if (!dbg_interface_result) {
    LOG_CRITICAL("Failed to create debug wireframe shader interface: {}",
                 dbg_interface_result.error().message);
    LUMINA_TERMINATE();
  }

  auto dbg_interface_index =
      renderer->AddShaderInterface(std::move(dbg_interface_result.value()));

  MaterialTemplateDescription dbg_mat_desc = {
      .shader_interface_index = dbg_interface_index,
      .vertex_layout = dbg_vert::kLayout,
      .fragment_layout = dbg_frag::kLayout,
      .vertex_shader_bin_path =
          lumina::common::PathRegistry::Instance()
              .shaders.Resolve("debug_wireframe.vert.spv")
              .string(),
      .fragment_shader_bin_path =
          lumina::common::PathRegistry::Instance()
              .shaders.Resolve("debug_wireframe.frag.spv")
              .string(),
      .max_instances = 0,
  };
  auto dbg_mat_template_handle =
      renderer->CreateMaterialTemplate(dbg_mat_desc);

  renderer->SetDebugWireframeMaterialTemplate(dbg_mat_template_handle);
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

auto GetFrameGlobalsBufferSize() -> VkDeviceSize {
  return sizeof(shaders::interface::global::FrameGlobals);
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

namespace {

// Shared by every write path: the only thing that differs between instances is
// which slice of the uniform buffer their descriptors point at.
auto WriteLitDescriptorsFor(LuminaRenderer *renderer,
                            const MaterialInstance &instance,
                            VkSampler sampler, VkImageView image_view) -> void {
  namespace mat = shaders::simple_input_basic_mat::frag;

  mat::BindingData bindings{
      .texSampler_sampler = sampler,
      .texSampler_imageView = image_view,
      .material_uniforms_buffer = renderer->GetMaterialUniformBuffer(),
      .material_uniforms_offset =
          renderer->GetMaterialUniformOffset(instance.GetUniformSlot()),
  };

  // Public accessor rather than the member: this helper is in an anonymous
  // namespace and so cannot be named in a friend declaration.
  auto *device = renderer->GetVulkanContext().GetDevice();
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    mat::WriteDescriptors(device, instance.GetDescriptorSet(i), bindings);
  }
}

} // namespace

auto SetLitMaterialDiffuseColor(LuminaRenderer *renderer,
                                MaterialInstanceHandle instance_handle,
                                const math::Vec3 &diffuse_color) -> void {
  using MU = shaders::simple_input_basic_mat::frag::MaterialUniforms;

  auto instance_opt =
      renderer->material_instance_manager.Get(instance_handle);
  LUMINA_CHECK(instance_opt.has_value(), "Material instance not found");

  auto *uniforms = static_cast<MU *>(
      renderer->GetMaterialUniformData(instance_opt.value()->GetUniformSlot()));
  uniforms->diffuse_color = diffuse_color;
}

auto WriteLitMaterialDescriptors(LuminaRenderer *renderer,
                                 MaterialInstanceHandle instance_handle)
    -> void {
  auto instance_opt =
      renderer->material_instance_manager.Get(instance_handle);
  LUMINA_CHECK(instance_opt.has_value(), "Material instance not found");

  WriteLitDescriptorsFor(renderer, *instance_opt.value(),
                         renderer->texture_sampler,
                         renderer->texture_image_view);
}

auto UpdateLitMaterialTextures(LuminaRenderer *renderer, VkSampler sampler,
                               VkImageView image_view) -> void {
  const auto lit_template = renderer->default_material_template_handle;

  renderer->material_instance_manager.ForEach(
      [renderer, sampler, image_view, lit_template](
          MaterialInstanceHandle /*handle*/,
          MaterialInstance &instance) -> void {
        // Instances of other templates have their own bindings and must not be
        // written with this layout's BindingData.
        if (instance.GetTemplateHandle() != lit_template) {
          return;
        }
        WriteLitDescriptorsFor(renderer, instance, sampler, image_view);
      });
}

auto CreateLitMaterialInstance(LuminaRenderer *renderer,
                               const math::Vec3 &diffuse_color)
    -> MaterialInstanceHandle {
  auto instance_handle = renderer->CreateMaterialInstance(
      renderer->default_material_template_handle);

  // Creation and descriptor-set allocation both land in the resource manager's
  // deferred queue, so each has to be flushed before the next step can read
  // back what it wrote.
  renderer->ProcessDeferredOperations();

  const bool allocated =
      renderer->AllocatePersistentDescriptorSets(instance_handle);
  LUMINA_CHECK(allocated,
               "Failed to allocate material instance descriptor sets — is "
               "max_instances large enough for the template?");
  renderer->ProcessDeferredOperations();

  WriteLitMaterialDescriptors(renderer, instance_handle);
  SetLitMaterialDiffuseColor(renderer, instance_handle, diffuse_color);
  return instance_handle;
}

auto WriteTransientDescriptors(LuminaRenderer *renderer,
                               FrameContext &frame_context,
                               VkDescriptorSet descriptor_set) -> void {
  namespace global = shaders::interface::global;
  global::WriteDescriptors(
      renderer->vulkan_context.GetDevice(), descriptor_set,
      {.frame_globals_buffer = frame_context.GetUniformBuffer().buffer});
}

} // namespace lumina::renderer