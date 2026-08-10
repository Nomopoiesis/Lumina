#include "vulkan_debug_callback.hpp"

#include "common/logger/logger.hpp"

#include <vulkan/vk_enum_string_helper.h>

namespace lumina::platform::common::vulkan {

namespace {

// vkGetInstanceProcAddr returns PFN_vkVoidFunction and the Vulkan spec requires
// the caller to cast it to the concrete entry point type. That is a genuine
// function-type mismatch as far as the compiler is concerned, so
// -Wcast-function-type-strict fires on it and there is nothing to fix. Funnel
// every extension lookup through here so the suppression lives in one place
// instead of at each call site.
template <typename Fn>
auto LoadInstanceFunction(VkInstance instance, const char *name) -> Fn {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
#endif
  return reinterpret_cast<Fn>(vkGetInstanceProcAddr(instance, name));
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

} // namespace

auto VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    -> VkBool32 {
  switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      LOG_TRACE("Validation layer: {}", pCallbackData->pMessage);
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      LOG_DEBUG("Validation layer: {}", pCallbackData->pMessage);
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      LOG_WARNING("Validation layer: {}", pCallbackData->pMessage);
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      LOG_ERROR("Validation layer: {}", pCallbackData->pMessage);
      break;
    default:
      LOG_DEBUG("Validation layer: {}", pCallbackData->pMessage);
      break;
  }
  return VK_FALSE;
}

auto CreateVulkanDebugMessenger(VkInstance instance) noexcept
    -> std::expected<VkDebugUtilsMessengerEXT, VkDebugMessengerCreationError> {
  VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = VulkanDebugCallback,
      .pUserData = nullptr,
  };

  auto Instace_vkCreateDebugUtilsMessengerEXT =
      LoadInstanceFunction<PFN_vkCreateDebugUtilsMessengerEXT>(
          instance, "vkCreateDebugUtilsMessengerEXT");
  if (Instace_vkCreateDebugUtilsMessengerEXT == nullptr) {
    return std::unexpected(VkDebugMessengerCreationError{
        .message = "Failed to get vkCreateDebugUtilsMessengerEXT function"});
  }

  if (auto result = Instace_vkCreateDebugUtilsMessengerEXT(
          instance, &debug_messenger_create_info, nullptr, &debug_messenger);
      result != VK_SUCCESS) {
    return std::unexpected(VkDebugMessengerCreationError{
        .message = "Failed to create Vulkan debug messenger with error code " +
                   std::to_string(result) + ": " +
                   std::string(string_VkResult(result))});
  }
  return debug_messenger;
}

auto DestroyVulkanDebugMessenger(
    VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger) noexcept
    -> void {
  if (debug_messenger == VK_NULL_HANDLE) {
    return;
  }
  auto Instace_vkDestroyDebugUtilsMessengerEXT =
      LoadInstanceFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(
          instance, "vkDestroyDebugUtilsMessengerEXT");
  if (Instace_vkDestroyDebugUtilsMessengerEXT == nullptr) {
    return;
  }
  Instace_vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
}
} // namespace lumina::platform::common::vulkan
