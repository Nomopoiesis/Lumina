#pragma once

#include <string>

#include <expected>

#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>
#undef VK_USE_PLATFORM_XCB_KHR

namespace lumina::platform::llinux::vulkan {

struct VkSurfaceCreationError {
  std::string message;
};

auto CreateVulkanSurface(VkInstance instance, xcb_window_t window_handle,
                         xcb_connection_t *connection)
    -> std::expected<VkSurfaceKHR, VkSurfaceCreationError>;

auto DestroyVulkanSurface(VkInstance instance, VkSurfaceKHR surface) -> void;

} // namespace lumina::platform::llinux::vulkan
