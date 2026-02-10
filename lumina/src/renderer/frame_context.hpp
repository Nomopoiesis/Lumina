#pragma once

#include <vulkan/vulkan.h>

#include "vulkan_context.hpp"

namespace lumina::renderer {

struct FrameContextCreationError {
  std::string message;
};

class FrameContext {
public:
  FrameContext(VulkanContext &vulkan_context) noexcept
      : vulkan_context(vulkan_context) {}
  ~FrameContext() noexcept;

  FrameContext(FrameContext &&other) noexcept;
  auto operator=(FrameContext &&other) noexcept -> FrameContext &;

  FrameContext(const FrameContext &) = delete;
  auto operator=(const FrameContext &) -> FrameContext & = delete;

  static auto Create(VulkanContext &vulkan_context, VkCommandPool command_pool)
      -> std::expected<FrameContext, FrameContextCreationError>;

  [[nodiscard]] auto GetCommandBuffer() const noexcept
      -> const VkCommandBuffer & {
    return command_buffer;
  }

  [[nodiscard]] auto GetFrameBeginSemaphore() const noexcept
      -> const VkSemaphore & {
    return frame_begin_semaphore;
  }

  [[nodiscard]] auto GetFrameBeginReadyFence() const noexcept
      -> const VkFence & {
    return frame_begin_ready_fence;
  }

private:
  VulkanContext &vulkan_context;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  VkSemaphore frame_begin_semaphore = VK_NULL_HANDLE;
  VkFence frame_begin_ready_fence = VK_NULL_HANDLE;
};

} // namespace lumina::renderer