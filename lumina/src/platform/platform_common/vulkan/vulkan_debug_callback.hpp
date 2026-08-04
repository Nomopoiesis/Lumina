#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <string>

namespace lumina::platform::common::vulkan {

struct VkDebugMessengerCreationError {
  std::string message;
};

VKAPI_ATTR auto VKAPI_CALL
VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData) -> VkBool32;

auto CreateVulkanDebugMessenger(VkInstance instance) noexcept
    -> std::expected<VkDebugUtilsMessengerEXT, VkDebugMessengerCreationError>;

auto DestroyVulkanDebugMessenger(
    VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger) noexcept
    -> void;

} // namespace lumina::platform::common::vulkan
