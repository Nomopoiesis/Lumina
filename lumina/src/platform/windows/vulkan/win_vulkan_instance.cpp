#include "win_vulkan_instance.hpp"

#include "lumina_types.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <cstring>
#include <vector>

namespace lumina::platform::windows::vulkan {

static auto
CheckValidationLayerSupport(const std::vector<const char *> &validation_layers)
    -> bool {
  u32 layer_count = 0;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  std::vector<VkLayerProperties> available_layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
  for (const auto &layer : validation_layers) {
    bool layer_found = false;
    for (const auto &available_layer : available_layers) {
      if (std::strcmp(layer, available_layer.layerName) == 0) {
        layer_found = true;
        break;
      }
    }
    if (!layer_found) {
      return false;
    }
  }
  return true;
}

auto CreateVulkanInstance()
    -> std::expected<VkInstance, VkInstanceCreationError> {
  // Application info
  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "Lumina",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Lumina Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_4,
  };

  // Required extensions for Windows
  std::vector<const char *> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
  };

  // Validation layers (only in debug builds)
  std::vector<const char *> validation_layers;
#ifndef NDEBUG
  validation_layers.push_back("VK_LAYER_KHRONOS_validation");
  if (!CheckValidationLayerSupport(validation_layers)) {
    return std::unexpected(VkInstanceCreationError{
        .message = "Requested Vulkan validation layers not supported"});
  }
  required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  // Instance create info
  VkInstanceCreateInfo instance_create_info{};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pApplicationInfo = &app_info;
  instance_create_info.enabledLayerCount =
      static_cast<u32>(validation_layers.size());
  instance_create_info.ppEnabledLayerNames = validation_layers.data();
  instance_create_info.enabledExtensionCount =
      static_cast<u32>(required_extensions.size());
  instance_create_info.ppEnabledExtensionNames = required_extensions.data();

  VkInstance instance = VK_NULL_HANDLE;
  if (auto result = vkCreateInstance(&instance_create_info, nullptr, &instance);
      result != VK_SUCCESS) {
    return std::unexpected(VkInstanceCreationError{
        .message = "Failed to create Vulkan instance with error code " +
                   std::to_string(result) + ": " +
                   std::string(string_VkResult(result))});
  }
  return instance;
}

auto DestroyVulkanInstance(VkInstance instance) -> void {
  if (instance != VK_NULL_HANDLE) {
    vkDestroyInstance(instance, nullptr);
  }
}

} // namespace lumina::platform::windows::vulkan
