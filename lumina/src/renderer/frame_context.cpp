#include "frame_context.hpp"

namespace lumina::renderer {

FrameContext::FrameContext(FrameContext &&other) noexcept
    : vulkan_context(other.vulkan_context),
      command_buffer(std::exchange(other.command_buffer, VK_NULL_HANDLE)),
      frame_begin_semaphore(
          std::exchange(other.frame_begin_semaphore, VK_NULL_HANDLE)),
      frame_begin_ready_fence(
          std::exchange(other.frame_begin_ready_fence, VK_NULL_HANDLE)) {}

auto FrameContext::operator=(FrameContext &&other) noexcept -> FrameContext & {
  if (this != &other) {
    vulkan_context = std::exchange(other.vulkan_context, VulkanContext());
    command_buffer = std::exchange(other.command_buffer, VK_NULL_HANDLE);
    frame_begin_semaphore =
        std::exchange(other.frame_begin_semaphore, VK_NULL_HANDLE);
    frame_begin_ready_fence =
        std::exchange(other.frame_begin_ready_fence, VK_NULL_HANDLE);
  }
  return *this;
}

FrameContext::~FrameContext() noexcept {
  if (frame_begin_semaphore != VK_NULL_HANDLE) {
    vkDestroySemaphore(vulkan_context.GetDevice(), frame_begin_semaphore,
                       nullptr);
  }
  if (frame_begin_ready_fence != VK_NULL_HANDLE) {
    vkDestroyFence(vulkan_context.GetDevice(), frame_begin_ready_fence,
                   nullptr);
  }
}

auto FrameContext::Create(VulkanContext &vulkan_context,
                          VkCommandPool command_pool)
    -> std::expected<FrameContext, FrameContextCreationError> {
  FrameContext frame_context(vulkan_context);
  auto command_buffer_result = vulkan_context.CreateCommandBuffer(command_pool);
  if (!command_buffer_result) {
    return std::unexpected(FrameContextCreationError{
        .message = "Failed to create command buffer"});
  }
  frame_context.command_buffer = command_buffer_result.value();

  auto frame_begin_semaphore_result = vulkan_context.CreateSemaphore();
  if (!frame_begin_semaphore_result) {
    return std::unexpected(FrameContextCreationError{
        .message = "Failed to create image available semaphore"});
  }
  frame_context.frame_begin_semaphore = frame_begin_semaphore_result.value();

  auto frame_begin_ready_fence_result = vulkan_context.CreateFence(true);
  if (!frame_begin_ready_fence_result) {
    return std::unexpected(FrameContextCreationError{
        .message = "Failed to create frame begin ready fence"});
  }
  frame_context.frame_begin_ready_fence =
      frame_begin_ready_fence_result.value();
  return frame_context;
}

} // namespace lumina::renderer