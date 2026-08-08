#pragma once

#include "material_instance_handle.hpp"
#include "math/vector.hpp"
#include "vulkan_context.hpp"

#include <expected>
#include <vector>
#include <vulkan/vulkan.h>

namespace lumina::renderer {

class LuminaRenderer;
class FrameContext;

auto BuildStaticMaterialTemplates(LuminaRenderer *renderer) -> void;

auto CreateGlobalDescriptorSetLayout(LuminaRenderer *renderer)
    -> std::expected<void, VkInitializationError>;

auto GetGlobalDescriptorPoolSizes(std::vector<VkDescriptorPoolSize> &pool_sizes)
    -> void;

// Returns sizeof(FrameGlobals) so renderer.cpp can create the buffer
// without including the generated header.
auto GetFrameGlobalsBufferSize() -> VkDeviceSize;

// Returns sizeof(MaterialUniforms) so renderer.cpp can create the buffer
// without including the generated header.
auto GetDefaultMaterialUBOSize() -> VkDeviceSize;

// Writes initial default values into a mapped material UBO buffer.
auto InitDefaultMaterialUBO(void *mapped_data) -> void;

// Points one instance's persistent descriptor sets (set 1) at the shared
// texture and at that instance's own slice of the material uniform buffer.
auto WriteLitMaterialDescriptors(LuminaRenderer *renderer,
                                 MaterialInstanceHandle instance_handle)
    -> void;

// Re-writes the texture binding of *every* lit material instance. All of them
// share one texture, so an upload completing has to reach all of them — not
// just the default, or later instances keep a stale image view.
// Safe to call from the render thread (e.g. a command-context completion
// callback).
auto UpdateLitMaterialTextures(LuminaRenderer *renderer, VkSampler sampler,
                               VkImageView image_view) -> void;

// Creates a ready-to-draw lit material instance with the given diffuse colour:
// claims a uniform slot, allocates descriptor sets, points them at that slot,
// and writes the colour.
//
// Setup only — it flushes the resource manager's deferred queue and writes
// descriptor sets, neither of which is safe while frames are in flight.
auto CreateLitMaterialInstance(LuminaRenderer *renderer,
                               const math::Vec3 &diffuse_color)
    -> MaterialInstanceHandle;

// Overwrites an existing instance's diffuse colour. Setup only: the GPU may be
// reading these uniforms during a frame.
auto SetLitMaterialDiffuseColor(LuminaRenderer *renderer,
                                MaterialInstanceHandle instance_handle,
                                const math::Vec3 &diffuse_color) -> void;

// Writes per-frame transient descriptor sets (set 0)
// using the generated WriteDescriptors function.
auto WriteTransientDescriptors(LuminaRenderer *renderer,
                               FrameContext &frame_context,
                               VkDescriptorSet descriptor_set) -> void;

} // namespace lumina::renderer