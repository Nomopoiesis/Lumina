#pragma once

#include "renderer.hpp"

namespace lumina::renderer {

auto BuildStaticMaterialTemplates(LuminaRenderer *renderer) -> void;

auto CreateGlobalDescriptorSetLayout(LuminaRenderer *renderer)
    -> std::expected<void, VkInitializationError>;

auto GetGlobalDescriptorPoolSizes(std::vector<VkDescriptorPoolSize> &pool_sizes)
    -> void;

// Returns sizeof(MaterialUniforms) so renderer.cpp can create the buffer
// without including the generated header.
auto GetDefaultMaterialUBOSize() -> VkDeviceSize;

// Writes initial default values into a mapped material UBO buffer.
auto InitDefaultMaterialUBO(void *mapped_data) -> void;

// Writes the default material's persistent descriptor sets (set 1)
// using the generated WriteDescriptors function.
auto WriteDefaultMaterialDescriptors(LuminaRenderer *renderer) -> void;

// Writes per-frame transient descriptor sets (set 0)
// using the generated WriteDescriptors function.
auto WriteTransientDescriptors(LuminaRenderer *renderer,
                               FrameContext &frame_context,
                               VkDescriptorSet descriptor_set) -> void;

} // namespace lumina::renderer