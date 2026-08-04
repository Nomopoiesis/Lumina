#include "linux_vulkan_surface.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::platform::llinux::vulkan {

auto CreateVulkanSurface(VkInstance instance, xcb_window_t window_handle,
                         xcb_connection_t *connection)
    -> std::expected<VkSurfaceKHR, VkSurfaceCreationError> {
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  VkXcbSurfaceCreateInfoKHR surface_create_info = {
      .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .connection = connection,
      .window = window_handle,
  };
  if (auto result = vkCreateXcbSurfaceKHR(instance, &surface_create_info,
                                          nullptr, &surface);
      result != VK_SUCCESS) {
    return std::unexpected(VkSurfaceCreationError{
        .message = "Failed to create Vulkan surface with error code " +
                   std::to_string(result) + ": " +
                   std::string(string_VkResult(result))});
  }
  return surface;
}

auto DestroyVulkanSurface(VkInstance instance, VkSurfaceKHR surface) -> void {
  if (surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance, surface, nullptr);
  }
}

} // namespace lumina::platform::llinux::vulkan
