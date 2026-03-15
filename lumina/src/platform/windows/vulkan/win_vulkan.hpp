#pragma once

#include <expected>
#include <string>

#include "win_window.hpp"

#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#include <vulkan/vulkan.h>
#undef WIN32_LEAN_AND_MEAN
#undef VK_USE_PLATFORM_WIN32_KHR

#include "common/vulkan/vulkan_init_result.hpp"

namespace lumina::platform::windows::vulkan {

struct VkInitializationError {
  std::string message;
};

auto InitializeVulkan(Window &window) noexcept
    -> std::expected<common::vulkan::VkInitializationResult,
                     VkInitializationError>;

auto DestroyVulkan(
    common::vulkan::VkInitializationResult &vulkan_init_result) noexcept
    -> void;

} // namespace lumina::platform::windows::vulkan
