#pragma once

#include "frame_context.hpp"
#include "lumina_types.hpp"
#include "ui_types.hpp"
#include "vulkan_context.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

// Owns the per-frame dynamic vertex/index buffers for the UI and records its
// draws. It no longer owns a pipeline, a pipeline layout, a descriptor set
// layout or a descriptor pool: the UI goes through the material template and
// graphics pipeline path like everything else, and its font atlas lives in the
// Screen2D family's set 0, which the frame context allocates.
class UIRenderer {
public:
  UIRenderer() noexcept = default;
  UIRenderer(const UIRenderer &) = delete;
  auto operator=(const UIRenderer &) -> UIRenderer & = delete;
  UIRenderer(UIRenderer &&) = delete;
  auto operator=(UIRenderer &&) -> UIRenderer & = delete;
  ~UIRenderer() noexcept = default;

  auto Initialize(VulkanContext &ctx) -> void;
  auto Shutdown(VulkanContext &ctx) -> void;

  // Record UI draw commands into cmd_buf for the current frame.
  // batch must remain valid until vkQueueSubmit for this frame.
  //
  // The caller supplies the resolved pipeline, its layout and the Screen2D
  // descriptor set, because resolving a pipeline handle reads the registries
  // the render thread mutates and that lookup belongs with the other record
  // time lookups.
  auto RecordCommands(VkCommandBuffer cmd_buf, u32 frame_index,
                      const UIRenderBatch &batch, u32 screen_width,
                      u32 screen_height, VkPipeline pipeline,
                      VkPipelineLayout pipeline_layout,
                      const std::vector<VkPushConstantRange> &push_ranges,
                      VkDescriptorSet font_atlas_set) -> void;

private:
  static constexpr u32 kMaxVertices = 65536;
  static constexpr u32 kMaxIndices = 262144;

  auto CreateBuffers(VulkanContext &ctx) -> void;

  // Per-frame-in-flight resources
  struct FrameResources {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
    void *vertex_mapped = nullptr;

    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
    void *index_mapped = nullptr;
  };
  FrameResources frame_resources[MAX_FRAMES_IN_FLIGHT];
};

} // namespace lumina::renderer
