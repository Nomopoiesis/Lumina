#pragma once

#include "linux_window.hpp"

#include "platform/platform_common/vulkan/vulkan_init_result.hpp"

namespace lumina::platform::llinux::vulkan {

struct VkInitializationError {
  std::string message;
};

auto InitializeVulkan(Window &window) noexcept
    -> std::expected<common::vulkan::VkInitializationResult,
                     VkInitializationError>;

auto DestroyVulkan(
    common::vulkan::VkInitializationResult &vulkan_init_result) noexcept
    -> void;

} // namespace lumina::platform::llinux::vulkan