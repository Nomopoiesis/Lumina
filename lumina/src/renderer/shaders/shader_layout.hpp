#pragma once

#include "core/vertex_attribute.hpp"
#include "lumina_types.hpp"

namespace lumina::renderer {

// Descriptor binding type — mirrors VkDescriptorType, backend-agnostic subset.
enum class DescriptorBindingType : u8 {
  Sampler,
  CombinedImageSampler,
  SampledImage,
  StorageImage,
  UniformTexelBuffer,
  StorageTexelBuffer,
  UniformBuffer,
  StorageBuffer,
};

enum class ShaderStage : u8 {
  Vertex,
  Fragment,
};

// Describes a single descriptor binding within a descriptor set.
// Populated at compile-time by generated .gen.hpp files; used at runtime to
// build VkDescriptorSetLayout and to write descriptor sets.
struct ShaderBindingInfo {
  u32 set = 0;
  u32 binding = 0;
  DescriptorBindingType type = DescriptorBindingType::UniformBuffer;
  const char *name = nullptr;
  u32 block_size = 0; // sizeof(UBO struct) for UniformBuffer; 0 otherwise
  u32 array_count = 1;
};

// Describes a single vertex shader input variable.
// location matches layout(location = N) in the vertex shader.
struct VertexInputInfo {
  u32 location;
  core::VertexAttributeType attribute_type;
  core::ElementType element_type;
};

// Describes all vertex input variables for a vertex shader stage.
// Populated only for vertex stage; empty for fragment stage.
struct VertexInputLayout {
  u32 input_count = 0;
  const VertexInputInfo *inputs = nullptr;
};

// Describes the full descriptor interface of one shader stage.
// Generated as a constexpr instance per stage in each .gen.hpp file.
struct ShaderLayout {
  ShaderStage stage = ShaderStage::Vertex;
  u32 binding_count = 0;
  const ShaderBindingInfo *bindings = nullptr;
  u32 push_constant_size = 0;   // bytes; 0 if no push constants
  u32 push_constant_offset = 0; // byte offset into the push constant buffer
  VertexInputLayout vertex_input_layout = {}; // populated only for vertex stage
};

} // namespace lumina::renderer
