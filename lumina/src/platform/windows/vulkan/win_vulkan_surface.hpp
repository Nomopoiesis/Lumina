#pragma once

#include <string>

#include <expected>

// VK_USE_PLATFORM_WIN32_KHR is defined for this whole directory by
// lumina/src/platform/windows/CMakeLists.txt. Defining it again here and then
// #undef-ing it stripped the definition for everything included afterwards,
// which would have silently dropped the Win32 surface declarations from any
// later <vulkan/vulkan.h>.
#include <Windows.h>
#include <vulkan/vulkan.h>

namespace lumina::platform::windows::vulkan {

struct VkSurfaceCreationError {
  std::string message;
};

auto CreateVulkanSurface(VkInstance instance, HWND window_handle,
                         HINSTANCE hinstance)
    -> std::expected<VkSurfaceKHR, VkSurfaceCreationError>;

auto DestroyVulkanSurface(VkInstance instance, VkSurfaceKHR surface) -> void;

} // namespace lumina::platform::windows::vulkan
