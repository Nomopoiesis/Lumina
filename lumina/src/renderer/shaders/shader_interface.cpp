#include "shader_interface.hpp"

#include "shader_vk_helpers.hpp"

#include <map>
#include <vector>

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::renderer {

struct BindingInfo {
  u32 binding;
  VkDescriptorType type;
  VkShaderStageFlags stage_flags;
  u32 array_count;
};

static auto CreateEmptyDescriptorSetLayout(VkDevice device)
    -> std::expected<VkDescriptorSetLayout, ShaderInterfaceCreateError> {
  const VkDescriptorSetLayoutCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 0,
  };
  VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
  if (auto r = vkCreateDescriptorSetLayout(device, &info, nullptr, &dsl);
      r != VK_SUCCESS) {
    return std::unexpected(ShaderInterfaceCreateError{
        .message = "Failed to create empty descriptor set layout: " +
                   std::string(string_VkResult(r))});
  }
  return dsl;
}

static auto
CreateDescriptorSetLayout(VkDevice device,
                          const std::vector<BindingInfo> &binding_infos)
    -> std::expected<VkDescriptorSetLayout, ShaderInterfaceCreateError> {

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(binding_infos.size());
  for (const auto &binding_info : binding_infos) {
    bindings.push_back({
        .binding = binding_info.binding,
        .descriptorType = binding_info.type,
        .descriptorCount = binding_info.array_count,
        .stageFlags = binding_info.stage_flags,
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

  VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
  if (auto result = vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
                                                &descriptor_set_layout);
      result != VK_SUCCESS) {
    return std::unexpected(ShaderInterfaceCreateError{
        .message = "Failed to create descriptor set layout with error code " +
                   std::to_string(result) + ": " +
                   std::string(string_VkResult(result))});
  }
  return descriptor_set_layout;
}

static auto CreatePipelineLayout(
    VkDevice device, std::vector<VkPushConstantRange> &push_constant_ranges,
    const std::vector<VkDescriptorSetLayout> &descriptor_set_layouts)
    -> std::expected<VkPipelineLayout, ShaderInterfaceCreateError> {

  VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = static_cast<u32>(descriptor_set_layouts.size()),
      .pSetLayouts = descriptor_set_layouts.data(),
      .pushConstantRangeCount = static_cast<u32>(push_constant_ranges.size()),
      .pPushConstantRanges = push_constant_ranges.data(),
  };

  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  if (auto result = vkCreatePipelineLayout(device, &pipeline_layout_create_info,
                                           nullptr, &pipeline_layout);
      result != VK_SUCCESS) {
    return std::unexpected(ShaderInterfaceCreateError{
        .message = "Failed to create pipeline layout with error code " +
                   std::to_string(result) + ": " +
                   std::string(string_VkResult(result))});
  }

  return pipeline_layout;
}

ShaderInterface::~ShaderInterface() noexcept {
  if (m_device != VK_NULL_HANDLE) {
    if (pipeline_layout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(m_device, pipeline_layout, nullptr);
    }
    for (size_t i = externally_owned_set_count;
         i < descriptor_set_layouts.size(); ++i) {
      if (descriptor_set_layouts[i] != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, descriptor_set_layouts[i],
                                     nullptr);
      }
    }
    m_device = VK_NULL_HANDLE;
  }
}

ShaderInterface::ShaderInterface(ShaderInterface &&other) noexcept
    : m_device(other.m_device), name(std::move(other.name)),
      descriptor_set_layouts(std::move(other.descriptor_set_layouts)),
      externally_owned_set_count(other.externally_owned_set_count),
      set_indices(std::move(other.set_indices)),
      vertex_input_layout(other.vertex_input_layout),
      pipeline_layout(other.pipeline_layout) {
  other.m_device = VK_NULL_HANDLE;
  other.name.clear();
  other.descriptor_set_layouts.clear();
  other.externally_owned_set_count = 0;
  other.set_indices.clear();
  other.vertex_input_layout = {};
  other.pipeline_layout = VK_NULL_HANDLE;
}

auto ShaderInterface::operator=(ShaderInterface &&other) noexcept
    -> ShaderInterface & {
  if (this != &other) {
    m_device = other.m_device;
    name = std::move(other.name);
    descriptor_set_layouts = std::move(other.descriptor_set_layouts);
    externally_owned_set_count = other.externally_owned_set_count;
    set_indices = std::move(other.set_indices);
    vertex_input_layout = other.vertex_input_layout;
    pipeline_layout = other.pipeline_layout;
    other.m_device = VK_NULL_HANDLE;
    other.name.clear();
    other.descriptor_set_layouts.clear();
    other.externally_owned_set_count = 0;
    other.set_indices.clear();
    other.vertex_input_layout = {};
    other.pipeline_layout = VK_NULL_HANDLE;
  }
  return *this;
}
auto ShaderInterface::Create(VkDevice device, const ShaderLayout &vertex_layout,
                             const ShaderLayout &fragment_layout,
                             const std::string &name,
                             VkDescriptorSetLayout global_descriptor_set_layout,
                             const ShaderLayout &global_layout)
    -> std::expected<ShaderInterface, ShaderInterfaceCreateError> {
  ShaderInterface shader_interface;
  shader_interface.m_device = device;
  shader_interface.name = name;

  // Slot the externally-owned global descriptor set layout at dense index 0.
  shader_interface.set_indices.push_back(0); // set_indices[0] = dense index 0
  shader_interface.descriptor_set_layouts.push_back(
      global_descriptor_set_layout);
  shader_interface.externally_owned_set_count = 1;

  std::map<std::pair<u32, u32>, BindingInfo> merged_binding_infos;
  for (u32 i = 0; i < vertex_layout.binding_count; ++i) {
    const auto &binding = vertex_layout.bindings[i];
    merged_binding_infos[{binding.set, binding.binding}] = {
        .binding = binding.binding,
        .type = ToVkDescriptorType(binding.type),
        .stage_flags = VK_SHADER_STAGE_VERTEX_BIT,
        .array_count = binding.array_count,
    };
  }
  for (u32 i = 0; i < fragment_layout.binding_count; ++i) {
    const auto &binding = fragment_layout.bindings[i];
    // If the binding already exists, add the fragment stage flags to the
    // existing binding info
    if (merged_binding_infos.contains({binding.set, binding.binding})) {
      merged_binding_infos[{binding.set, binding.binding}].stage_flags |=
          VK_SHADER_STAGE_FRAGMENT_BIT;
    } else {
      merged_binding_infos[{binding.set, binding.binding}] = {
          .binding = binding.binding,
          .type = ToVkDescriptorType(binding.type),
          .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .array_count = binding.array_count,
      };
    }
  }

  // Gather all bindings in a set
  std::map<u32, std::vector<BindingInfo>> set_binding_infos;
  for (const auto &[binding_pair, binding_info] : merged_binding_infos) {
    auto [set, binding] = binding_pair;
    set_binding_infos[set].push_back(binding_info);
  }

  for (const auto &[set, binding_infos] : set_binding_infos) {
    // Set 0 is the global descriptor set — already provided externally.
    if (set == 0) {
      continue;
    }
    auto descriptor_set_layout_result =
        CreateDescriptorSetLayout(device, binding_infos);
    if (!descriptor_set_layout_result) {
      return std::unexpected(descriptor_set_layout_result.error());
    }
    // set_indices[sparse_set] = dense_index; extend with UINT32_MAX for gaps
    if (set >= shader_interface.set_indices.size()) {
      shader_interface.set_indices.resize(set + 1, UINT32_MAX);
    }
    shader_interface.set_indices[set] =
        static_cast<u32>(shader_interface.descriptor_set_layouts.size());
    shader_interface.descriptor_set_layouts.push_back(
        descriptor_set_layout_result.value());
  }

  // Push constants are global — sourced from the global layout so that all
  // pipeline layouts share the same push constant range.
  std::vector<VkPushConstantRange> push_constant_ranges;
  if (global_layout.push_constant_size > 0) {
    push_constant_ranges.push_back({
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = global_layout.push_constant_offset,
        .size = global_layout.push_constant_size,
    });
  }

  // Build a set-indexed layout vector for vkCreatePipelineLayout.
  // pSetLayouts[i] must be the layout for set i not the i-th populated set.
  // Gaps between used sets must be filled with empty (0-binding) DSLs since
  // VK_NULL_HANDLE is not a valid pSetLayouts entry.
  std::vector<VkDescriptorSetLayout> ordered_layouts;
  if (!shader_interface.set_indices.empty()) {
    const u32 max_set =
        static_cast<u32>(shader_interface.set_indices.size()) - 1;
    ordered_layouts.resize(max_set + 1, VK_NULL_HANDLE);
    for (u32 s = 0; s <= max_set; ++s) {
      if (shader_interface.set_indices[s] != UINT32_MAX) {
        ordered_layouts[s] =
            shader_interface
                .descriptor_set_layouts[shader_interface.set_indices[s]];
      }
    }

    for (u32 s = 0; s <= max_set; ++s) {
      if (ordered_layouts[s] != VK_NULL_HANDLE) {
        continue;
      }
      auto gap_result = CreateEmptyDescriptorSetLayout(device);
      if (!gap_result) {
        return std::unexpected(gap_result.error());
      }
      ordered_layouts[s] = gap_result.value();
      shader_interface.descriptor_set_layouts.push_back(gap_result.value());
    }
  }

  // Create pipeline layout
  auto pipeline_layout_result =
      CreatePipelineLayout(device, push_constant_ranges, ordered_layouts);
  if (!pipeline_layout_result) {
    return std::unexpected(pipeline_layout_result.error());
  }
  shader_interface.pipeline_layout = pipeline_layout_result.value();

  shader_interface.vertex_input_layout = vertex_layout.vertex_input_layout;

  return shader_interface;
}

} // namespace lumina::renderer