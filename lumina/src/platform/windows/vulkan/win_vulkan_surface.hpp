#pragma once

#include <string>

#include <expected>

#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>

#undef VK_USE_PLATFORM_WIN32_KHR

namespace lumina::platform::windows::vulkan {

struct VkSurfaceCreationError {
  std::string message;
};

auto CreateVulkanSurface(VkInstance instance, HWND window_handle,
                         HINSTANCE hinstance)
    -> std::expected<VkSurfaceKHR, VkSurfaceCreationError>;

auto DestroyVulkanSurface(VkInstance instance, VkSurfaceKHR surface) -> void;

} // namespace lumina::platform::windows::vulkan
