#include "win_vulkan.hpp"

#include "win_vulkan_instance.hpp"
#include "win_vulkan_surface.hpp"

#include "common/vulkan/vulkan_debug_callback.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::platform::windows::vulkan {

auto InitializeVulkan(Window &window) noexcept
    -> std::expected<common::vulkan::VkInitializationResult,
                     VkInitializationError> {
  std::string error_message;
  auto instance_result = CreateVulkanInstance();
  if (instance_result) {

    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
#ifndef NDEBUG
    auto debug_messenger_result =
        common::vulkan::CreateVulkanDebugMessenger(instance_result.value());
    if (debug_messenger_result) {
      debug_messenger = debug_messenger_result.value();
    } else {
      return std::unexpected(VkInitializationError{
          .message = "Failed to create Vulkan debug messenger: " +
                     debug_messenger_result.error().message});
    }
#endif

    auto surface_result =
        CreateVulkanSurface(instance_result.value(), window.GetWindowHandle(),
                            window.GetHInstance());
    if (surface_result) {
      return common::vulkan::VkInitializationResult{
          .instance = instance_result.value(),
          .surface = surface_result.value(),
          .debug_messenger = debug_messenger};
    } else {
      // We succesfully created the instance, but failed to create the surface
      // so we need to destroy the instance
      DestroyVulkanInstance(instance_result.value());
      error_message = std::move(surface_result.error().message);
    }
  } else {
    error_message = std::move(instance_result.error().message);
  }

  return std::unexpected(VkInitializationError{
      .message = "Failed to initialize Vulkan: " + error_message});
}

auto DestroyVulkan(
    common::vulkan::VkInitializationResult &vulkan_init_result) noexcept
    -> void {
  DestroyVulkanSurface(vulkan_init_result.instance, vulkan_init_result.surface);
  common::vulkan::DestroyVulkanDebugMessenger(
      vulkan_init_result.instance, vulkan_init_result.debug_messenger);
  DestroyVulkanInstance(vulkan_init_result.instance);
}

} // namespace lumina::platform::windows::vulkan
