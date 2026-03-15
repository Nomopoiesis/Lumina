#pragma once

#include <string>

#include <expected>

#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>
#undef VK_USE_PLATFORM_WIN32_KHR

namespace lumina::platform::windows::vulkan {

struct VkInstanceCreationError {
  std::string message;
};

auto CreateVulkanInstance()
    -> std::expected<VkInstance, VkInstanceCreationError>;

auto DestroyVulkanInstance(VkInstance instance) -> void;

} // namespace lumina::platform::windows::vulkan
