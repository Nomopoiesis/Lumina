#pragma once

#include "platform/common/vulkan/vulkan_init_result.hpp"

#include "frame_context.hpp"
#include "vulkan_context.hpp"

#include <semaphore>
#include <thread>
#include <vector>

namespace lumina::renderer {

class LuminaRenderer {
public:
  LuminaRenderer(platform::common::vulkan::VkInitializationResult
                     vulkan_init_result) noexcept;

  LuminaRenderer(const LuminaRenderer &) = delete;
  auto operator=(const LuminaRenderer &) -> LuminaRenderer & = delete;
  LuminaRenderer(LuminaRenderer &&) noexcept = delete;
  auto operator=(LuminaRenderer &&) noexcept -> LuminaRenderer & = delete;

  ~LuminaRenderer() noexcept;

  auto Initialize() -> void;

  auto DrawFrame() -> void;

  auto Shutdown() -> void;

  auto DeviceWaitIdle() -> void;

  auto SetFramebufferResized(bool resized) -> void {
    is_framebuffer_resized = resized;
  }
  [[nodiscard]] auto IsFramebufferResized() const -> bool {
    return is_framebuffer_resized;
  }

  // Acquire a frame context for update (API exposed to the engine to coordinate
  // the frame update)
  auto AcquireFrameContextForUpdate() -> void;
  // Release a frame context for update (API exposed to the engine to coordinate
  // the frame update)
  auto ReleaseFrameContextForUpdate() -> void;

  [[nodiscard]] auto GetFrameContextForUpdate() noexcept -> FrameContext * {
    return frame_context_for_update;
  }

private:
  auto RenderThread() -> void;

  auto AcquireFrameContextForRender() -> void;
  auto ReleaseFrameContextForRender() -> void;

  auto TryReclaimFrameContexts() -> void;

  auto CreatePipelineLayout() -> std::expected<void, VkInitializationError>;
  auto CreatePipeline() -> std::expected<void, VkInitializationError>;

  auto RecordCommandBuffer(FrameContext &frame_context,
                           u32 image_index) noexcept
      -> std::expected<void, VkInitializationError>;

  size_t current_frame_index = 0;

  bool is_framebuffer_resized = false;

  bool shutdown_requested = false;

  VulkanContext vulkan_context;

  std::thread render_thread;

  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> descriptor_sets;

  VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;

  std::counting_semaphore<MAX_FRAMES_IN_FLIGHT> frames_available_for_update{
      MAX_FRAMES_IN_FLIGHT};
  std::counting_semaphore<MAX_FRAMES_IN_FLIGHT> frames_available_for_render{0};

  FrameContext *frame_context_for_update = nullptr;
  FrameContext *frame_context_for_render = nullptr;

  std::vector<FrameContext> frame_contexts;

  VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
  VkBuffer vertex_buffer = VK_NULL_HANDLE;
  VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
  VkBuffer index_buffer = VK_NULL_HANDLE;

  VkDeviceMemory texture_image_memory = VK_NULL_HANDLE;
  VkImage texture_image = VK_NULL_HANDLE;
  VkImageView texture_image_view = VK_NULL_HANDLE;
  VkSampler texture_sampler = VK_NULL_HANDLE;

  VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
  VkDeviceMemory depth_image_memory = VK_NULL_HANDLE;
  VkImage depth_image = VK_NULL_HANDLE;
  VkImageView depth_image_view = VK_NULL_HANDLE;
};

} // namespace lumina::renderer
