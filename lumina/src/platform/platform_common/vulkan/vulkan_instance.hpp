#pragma once

#include "lumina_types.hpp"

#include <string>
#include <vector>

#include <expected>

#include <vulkan/vulkan.h>

namespace lumina::platform::common::vulkan {

struct VulkanInstanceCreateInfo {
  std::string application_name;
  u32 application_version_major;
  u32 application_version_minor;
  u32 application_version_patch;
  std::vector<const char *> required_extensions;
};

struct VkInstanceCreationError {
  std::string message;
};

auto CreateVulkanInstance(VulkanInstanceCreateInfo &create_info)
    -> std::expected<VkInstance, VkInstanceCreationError>;

auto DestroyVulkanInstance(VkInstance instance) -> void;

} // namespace lumina::platform::common::vulkan
