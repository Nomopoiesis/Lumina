#pragma once

#include <vulkan/vulkan.h>

namespace lumina::platform::common::vulkan {

/**
 * @brief Result of initializing Vulkan
 *
 * @details Contains the initialized context, instance, and surface of the
 * Vulkan instance.
 * @note We use Vulkan RAII to manage the Vulkan instance and surface. When
 * those variables go out of scope, they will be properly destroyed by the
 * Vulkan RAII wrappers. Thus the order of these variables matters, as the
 * Surface needs to be destroyed before the Instance and Instance must be
 * destroyed before the Context.
 */
struct VkInitializationResult {
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
};

} // namespace lumina::platform::common::vulkan
