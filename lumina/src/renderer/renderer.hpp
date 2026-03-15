#pragma once

#include "platform/common/vulkan/vulkan_init_result.hpp"

#include "frame_context.hpp"
#include "vulkan_context.hpp"

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

  auto DeviceWaitIdle() -> void;

  auto SetFramebufferResized(bool resized) -> void {
    is_framebuffer_resized = resized;
  }
  [[nodiscard]] auto IsFramebufferResized() const -> bool {
    return is_framebuffer_resized;
  }

private:
  auto CreatePipelineLayout() -> std::expected<void, VkInitializationError>;
  auto CreatePipeline() -> std::expected<void, VkInitializationError>;

  auto RecordCommandBuffer(FrameContext &frame_context,
                           u32 image_index) noexcept
      -> std::expected<void, VkInitializationError>;

  static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
  size_t current_frame_index = 0;

  bool is_framebuffer_resized = false;

  VulkanContext vulkan_context;

  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;

  std::vector<FrameContext> frame_contexts;
};

} // namespace lumina::renderer
