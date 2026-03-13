#include "command_context.hpp"

#include "logger/logger.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::renderer {
CommandContext::~CommandContext() noexcept {
  if (command_buffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(vulkan_context->GetDevice(), command_pool, 1,
                         &command_buffer);
  }
  if (command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(vulkan_context->GetDevice(), command_pool, nullptr);
  }
  Reset();
  vulkan_context = nullptr;
}

auto CommandContext::Initialize(VulkanContext &ctx) noexcept -> void {
  this->vulkan_context = &ctx;
  auto command_pool_result = vulkan_context->CreateCommandPool();
  if (!command_pool_result) {
    LOG_CRITICAL("Failed to create command pool: {}",
                 command_pool_result.error().message);
    LUMINA_TERMINATE();
  }
  command_pool = command_pool_result.value();
  auto command_buffer_result =
      vulkan_context->CreateCommandBuffer(command_pool);
  if (!command_buffer_result) {
    LOG_CRITICAL("Failed to create command buffer: {}",
                 command_buffer_result.error().message);
    LUMINA_TERMINATE();
  }
  command_buffer = command_buffer_result.value();
}

auto CommandContext::Begin() -> void {
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };

  if (auto result = vkBeginCommandBuffer(command_buffer, &begin_info);
      result != VK_SUCCESS) {
    LOG_ERROR("Failed to begin command buffer: {}", string_VkResult(result));
    LUMINA_TERMINATE();
  }
}

auto CommandContext::End() -> void {
  if (auto result = vkEndCommandBuffer(command_buffer); result != VK_SUCCESS) {
    LOG_ERROR("Failed to end command buffer: {}", string_VkResult(result));
    LUMINA_TERMINATE();
  }
}
} // namespace lumina::renderer