#pragma once

#include "frame_context.hpp"
#include "lumina_types.hpp"
#include "ui_types.hpp"
#include "vulkan_context.hpp"

#include <vulkan/vulkan.h>

namespace lumina::renderer {

// Manages the 2D UI rendering pipeline and per-frame dynamic vertex/index
// buffers. Operates entirely within the existing dynamic rendering pass.
class UIRenderer {
public:
  UIRenderer() noexcept = default;
  UIRenderer(const UIRenderer &) = delete;
  auto operator=(const UIRenderer &) -> UIRenderer & = delete;
  UIRenderer(UIRenderer &&) = delete;
  auto operator=(UIRenderer &&) -> UIRenderer & = delete;
  ~UIRenderer() noexcept = default;

  auto Initialize(VulkanContext &ctx, const std::string &vert_spv_path,
                  const std::string &frag_spv_path, VkFormat depth_format)
      -> void;
  auto Shutdown(VulkanContext &ctx) -> void;

  // Called from the render-thread completion callback once the font atlas
  // upload finishes. Updates all per-frame descriptor sets.
  auto SetFontAtlas(VulkanContext &ctx, VkImageView image_view,
                    VkSampler sampler) -> void;

  // Record UI draw commands into cmd_buf for the current frame.
  // batch must remain valid until vkQueueSubmit for this frame.
  auto RecordCommands(VkCommandBuffer cmd_buf, u32 frame_index,
                      const UIRenderBatch &batch, u32 screen_width,
                      u32 screen_height, VulkanContext &ctx) -> void;

private:
  static constexpr u32 kMaxVertices = 65536;
  static constexpr u32 kMaxIndices = 262144;

  auto CreatePipeline(VulkanContext &ctx, const std::string &vert_spv_path,
                      const std::string &frag_spv_path, VkFormat depth_format)
      -> void;
  auto CreateBuffers(VulkanContext &ctx) -> void;
  auto CreateDescriptors(VulkanContext &ctx) -> void;

  VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

  // Per-frame-in-flight resources
  struct FrameResources {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
    void *vertex_mapped = nullptr;

    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
    void *index_mapped = nullptr;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  };
  FrameResources frame_resources[MAX_FRAMES_IN_FLIGHT];

  bool font_atlas_bound = false;
};

} // namespace lumina::renderer
