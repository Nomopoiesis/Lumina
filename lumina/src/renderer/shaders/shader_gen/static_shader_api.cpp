#include "static_shader_api.hpp"

#include "common/lumina_check.hpp"
#include "common/path_registry.hpp"
#include "headers/color_output.frag.hpp"
#include "headers/lit_mesh.frag.hpp"
#include "headers/lit_mesh.vert.hpp"
#include "headers/pick_id.frag.hpp"
#include "headers/pick_id.vert.hpp"
#include "headers/position_only.vert.hpp"
#include "headers/scene3d.global.hpp"
#include "headers/screen2d.global.hpp"
#include "headers/ui_2d.frag.hpp"
#include "headers/ui_2d.vert.hpp"
#include "renderer.hpp"

#include <iterator>

#include "vk_helpers.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::renderer {

auto BuildStaticMaterialTemplates(LuminaRenderer *renderer) -> void {
  namespace global = lumina::shaders::scene3d::global;
  namespace vert = lumina::shaders::lit_mesh::vert;
  namespace frag = lumina::shaders::lit_mesh::frag;

  auto shader_interface_result = ShaderInterface::Create(
      renderer->GetVulkanContext().GetDevice(), vert::kLayout, frag::kLayout,
      "standard_lit",
      renderer->GetGlobalDescriptorSetLayout(GlobalShaderFamily::Scene3D));
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
      renderer->GetGlobalDescriptorSetLayout(GlobalShaderFamily::Scene3D));
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
      .vertex_shader_bin_path = lumina::common::PathRegistry::Instance()
                                    .shaders.Resolve("debug_wireframe.vert.spv")
                                    .string(),
      .fragment_shader_bin_path =
          lumina::common::PathRegistry::Instance()
              .shaders.Resolve("debug_wireframe.frag.spv")
              .string(),
      .max_instances = 0,
  };
  auto dbg_mat_template_handle = renderer->CreateMaterialTemplate(dbg_mat_desc);

  renderer->SetDebugWireframeMaterialTemplate(dbg_mat_template_handle);

  namespace pick_id_vert = lumina::shaders::pick_id::vert;
  namespace pick_id_frag = lumina::shaders::pick_id::frag;
  auto pick_interface_result = ShaderInterface::Create(
      renderer->GetVulkanContext().GetDevice(), pick_id_vert::kLayout,
      pick_id_frag::kLayout, "pick_id",
      renderer->GetGlobalDescriptorSetLayout(GlobalShaderFamily::Scene3D));
  if (!pick_interface_result) {
    LOG_CRITICAL("Failed to create pick id shader interface: {}",
                 pick_interface_result.error().message);
    LUMINA_TERMINATE();
  }
  auto pick_interface_index =
      renderer->AddShaderInterface(std::move(pick_interface_result.value()));

  MaterialTemplateDescription pick_id_mat_desc = {
      .shader_interface_index = pick_interface_index,
      .vertex_layout = pick_id_vert::kLayout,
      .fragment_layout = pick_id_frag::kLayout,
      .vertex_shader_bin_path = lumina::common::PathRegistry::Instance()
                                    .shaders.Resolve("pick_id.vert.spv")
                                    .string(),
      .fragment_shader_bin_path = lumina::common::PathRegistry::Instance()
                                      .shaders.Resolve("pick_id.frag.spv")
                                      .string(),
      .max_instances = 0,
  };
  auto pick_id_mat_template_handle =
      renderer->CreateMaterialTemplate(pick_id_mat_desc);

  renderer->SetPickIdMaterialTemplate(pick_id_mat_template_handle);

  // Screen-space UI. The only template in the Screen2D family, and the only one
  // whose set 0 is not the scene's — which is the whole reason the family
  // exists: the font atlas is shared by every UI draw, so it belongs in the
  // pass's global set rather than in a per-material one.
  //
  // No set 1 and no material instances: ui_2d::frag declares no bindings of its
  // own, so max_instances stays 0 and nothing claims a material uniform slot.
  namespace ui_vert = lumina::shaders::ui_2d::vert;
  namespace ui_frag = lumina::shaders::ui_2d::frag;
  auto ui_interface_result = ShaderInterface::Create(
      renderer->GetVulkanContext().GetDevice(), ui_vert::kLayout,
      ui_frag::kLayout, "ui_2d",
      renderer->GetGlobalDescriptorSetLayout(GlobalShaderFamily::Screen2D));
  if (!ui_interface_result) {
    LOG_CRITICAL("Failed to create UI shader interface: {}",
                 ui_interface_result.error().message);
    LUMINA_TERMINATE();
  }
  auto ui_interface_index =
      renderer->AddShaderInterface(std::move(ui_interface_result.value()));

  MaterialTemplateDescription ui_mat_desc = {
      .shader_interface_index = ui_interface_index,
      .vertex_layout = ui_vert::kLayout,
      .fragment_layout = ui_frag::kLayout,
      .vertex_shader_bin_path = lumina::common::PathRegistry::Instance()
                                    .shaders.Resolve("ui.vert.spv")
                                    .string(),
      .fragment_shader_bin_path = lumina::common::PathRegistry::Instance()
                                      .shaders.Resolve("ui.frag.spv")
                                      .string(),
      .max_instances = 0,
  };
  renderer->SetUIMaterialTemplate(
      renderer->CreateMaterialTemplate(ui_mat_desc));
}

namespace {

// Both families' set 0 layouts are built the same way — only the reflected
// binding list differs — so the family table is the one place that names them.
struct GlobalShaderFamilyBindings {
  GlobalShaderFamily family;
  const ShaderBindingInfo *bindings;
  u32 binding_count;
};

constexpr GlobalShaderFamilyBindings kGlobalShaderFamilyBindings[] = {
    {GlobalShaderFamily::Scene3D, lumina::shaders::scene3d::global::kBindings,
     lumina::shaders::scene3d::global::kLayout.binding_count},
    {GlobalShaderFamily::Screen2D, lumina::shaders::screen2d::global::kBindings,
     lumina::shaders::screen2d::global::kLayout.binding_count},
};

static_assert(
    std::size(kGlobalShaderFamilyBindings) == kGlobalShaderFamilyCount,
    "Every GlobalShaderFamily needs a binding list, or its set 0 layout is "
    "never created and every shader in it binds a null set");

} // namespace

auto CreateGlobalDescriptorSetLayout(LuminaRenderer *renderer)
    -> std::expected<void, VkInitializationError> {
  auto *device = renderer->vulkan_context.GetDevice();

  for (const auto &family : kGlobalShaderFamilyBindings) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(family.binding_count);
    for (u32 i = 0; i < family.binding_count; ++i) {
      const auto &b = family.bindings[i];
      bindings.push_back({
          .binding = b.binding,
          .descriptorType = ToVkDescriptorType(b.type),
          .descriptorCount = b.array_count,
          // Both stages, rather than the stage that declared it: the layout is
          // shared by every shader in the family, and they do not agree on
          // which stage reads what.
          .stageFlags =
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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

    if (auto result = vkCreateDescriptorSetLayout(
            device, &layout_info, nullptr,
            &renderer
                 ->global_descriptor_set_layouts[FamilyIndex(family.family)]);
        result != VK_SUCCESS) {
      return std::unexpected(VkInitializationError{
          .message = "Failed to create global descriptor set layout: " +
                     std::string(string_VkResult(result))});
    }
  }

  // There is deliberately no standalone "set 0 only" pipeline layout here any
  // more. Layout compatibility compares push constant ranges as well as
  // descriptor set layouts, and ranges are now per shader, so no single layout
  // can be compatible with every pipeline that shares this set. One that was
  // compatible with only some of them would bind correctly right up until it
  // silently did not. Set 0 is bound through each shader interface's own
  // pipeline layout instead — see RecordCommandBuffer.
  return {};
}

// Sized for every family, because one transient pool per frame serves them all.
auto GetGlobalDescriptorPoolSizes(std::vector<VkDescriptorPoolSize> &pool_sizes)
    -> void {
  for (const auto &family : kGlobalShaderFamilyBindings) {
    for (u32 i = 0; i < family.binding_count; ++i) {
      const auto &binding = family.bindings[i];
      pool_sizes.push_back({
          .type = ToVkDescriptorType(binding.type),
          .descriptorCount = binding.array_count,
      });
    }
  }
}

auto GetFrameGlobalsBufferSize() -> VkDeviceSize {
  return sizeof(shaders::scene3d::global::FrameGlobals);
}

auto GetLightingBufferSize() -> VkDeviceSize {
  return sizeof(shaders::scene3d::global::Lighting);
}

auto GetDefaultMaterialUBOSize() -> VkDeviceSize {
  return sizeof(shaders::lit_mesh::frag::MaterialUniforms);
}

auto InitDefaultMaterialUBO(void *mapped_data) -> void {
  using MU = shaders::lit_mesh::frag::MaterialUniforms;
  auto *mu = static_cast<MU *>(mapped_data);
  mu->ambient_intensity = 0.05F;
  mu->ambient_color = {1.0F, 1.0F, 1.0F};
  mu->diffuse_color = {0.5F, 0.5F, 0.5F};
}

namespace {

// Shared by every write path: the only thing that differs between instances is
// which slice of the uniform buffer their descriptors point at.
auto WriteLitDescriptorsFor(LuminaRenderer *renderer,
                            const MaterialInstance &instance, VkSampler sampler,
                            VkImageView image_view) -> void {
  namespace mat = shaders::lit_mesh::frag;

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
  using MU = shaders::lit_mesh::frag::MaterialUniforms;

  auto instance_opt = renderer->material_instance_manager.Get(instance_handle);
  LUMINA_CHECK(instance_opt.has_value(), "Material instance not found");

  auto *uniforms = static_cast<MU *>(
      renderer->GetMaterialUniformData(instance_opt.value()->GetUniformSlot()));
  uniforms->diffuse_color = diffuse_color;
}

auto WriteLitMaterialDescriptors(LuminaRenderer *renderer,
                                 MaterialInstanceHandle instance_handle)
    -> void {
  auto instance_opt = renderer->material_instance_manager.Get(instance_handle);
  LUMINA_CHECK(instance_opt.has_value(), "Material instance not found");

  WriteLitDescriptorsFor(renderer, *instance_opt.value(),
                         renderer->texture_sampler,
                         renderer->texture_image_view);
}

auto UpdateLitMaterialTextures(LuminaRenderer *renderer, VkSampler sampler,
                               VkImageView image_view) -> void {
  const auto lit_template = renderer->default_material_template_handle;

  renderer->material_instance_manager.ForEach(
      [renderer, sampler, image_view,
       lit_template](MaterialInstanceHandle /*handle*/,
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

// The Screen2D family's set 0. Separate from WriteTransientDescriptors because
// it has no per-frame inputs at all — the atlas view and sampler are whatever
// the upload published — and because it must not be written before that upload
// completes.
auto WriteUIDescriptors(LuminaRenderer *renderer,
                        VkDescriptorSet descriptor_set) -> void {
  namespace ui_global = shaders::screen2d::global;
  ui_global::WriteDescriptors(
      renderer->vulkan_context.GetDevice(), descriptor_set,
      {.font_atlas_sampler = renderer->ui_font_atlas_sampler,
       .font_atlas_imageView = renderer->ui_font_atlas_image_view});
}

auto WriteTransientDescriptors(LuminaRenderer *renderer,
                               FrameContext &frame_context,
                               VkDescriptorSet descriptor_set) -> void {
  namespace global = shaders::scene3d::global;
  auto &scene3d = frame_context.GetShaderFamilyScene3DResources();
  global::WriteDescriptors(
      renderer->vulkan_context.GetDevice(), descriptor_set,
      {.frame_globals_buffer = scene3d.frame_globals.buffer,
       .instance_data_buffer = frame_context.GetInstanceBuffer().buffer,
       .lighting_buffer = scene3d.lighting.buffer});
}

} // namespace lumina::renderer
