#pragma once

#include <vulkan/vulkan.h>

#include "vulkan_context.hpp"

#include <functional>
#include <vector>

namespace lumina::renderer {

class CommandContext {
public:
  CommandContext() noexcept = default;
  CommandContext(const CommandContext &other) noexcept = delete;
  CommandContext(CommandContext &&other) noexcept = delete;
  auto operator=(const CommandContext &other) noexcept
      -> CommandContext & = delete;
  auto operator=(CommandContext &&other) noexcept -> CommandContext & = delete;
  ~CommandContext() noexcept;

  auto Initialize(VulkanContext &ctx) noexcept -> void;

  auto Begin() -> void;
  auto End() -> void;

  auto
  AddCompletionCallback(const std::function<void(CommandContext *)> &callback)
      -> void {
    completion_callbacks.push_back(callback);
  }

  auto RunCompletionCallbacks() -> void {
    for (auto &callback : completion_callbacks) {
      callback(this);
    }
  }

  auto Reset() -> void { completion_callbacks.clear(); }

  VulkanContext *vulkan_context = nullptr;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  std::vector<std::function<void(CommandContext *)>> completion_callbacks;
};

} // namespace lumina::renderer
