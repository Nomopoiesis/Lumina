#include "vulkan_context.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <vector>

#include "core/lumina_engine.hpp"

namespace lumina::renderer {

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

static auto QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
    -> SwapChainSupportDetails {
  SwapChainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &details.capabilities);
  u32 format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
  if (format_count != 0) {
    details.formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                         details.formats.data());
  }

  u32 present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                            &present_mode_count, nullptr);
  if (present_mode_count != 0) {
    details.present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &present_mode_count, details.present_modes.data());
  }

  return details;
}

static auto
ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
    -> VkSurfaceFormatKHR {
  for (const auto &format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }
  return formats[0];
}

static auto
ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &present_modes)
    -> VkPresentModeKHR {
  for (const auto &present_mode : present_modes) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return present_mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

static auto ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
    -> VkExtent2D {
  if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
    return capabilities.currentExtent;
  }

  auto dim = core::LuminaEngine::Instance().GetWindowDimensions();
  VkExtent2D extent = {dim.width, dim.height};
  extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width);
  extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height);
  return extent;
}

static auto FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    -> VulkanContext::QueueFamilyIndices {
  VulkanContext::QueueFamilyIndices indices;
  u32 queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_families.data());
  for (u32 i = 0; i < queue_family_count; i++) {
    if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
      indices.graphics_family = i;
    }
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
    if (present_support == VK_TRUE) {
      indices.present_family = i;
    }

    if (indices.IsComplete()) {
      break;
    }
  }
  return indices;
}

static auto
CalculateDeviceScore(const VkPhysicalDeviceProperties &device_properties)
    -> u32 {
  u32 score = 0;
  if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  }
  if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
    score += 500;
  }
  if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
    score += 100;
  }

  return score;
}

static auto CheckDeviceExtensionsSupport(
    VkPhysicalDevice device,
    const std::vector<const char *> &required_extensions) -> bool {
  u32 extension_count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       nullptr);
  std::vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                       extensions.data());
  std::set<std::string> check_extensions{required_extensions.begin(),
                                         required_extensions.end()};
  for (const auto &extension : extensions) {
    check_extensions.erase(std::string(extension.extensionName));
  }
  return check_extensions.empty();
}

static auto
IsDeviceSuitable(VkPhysicalDevice device,
                 const VkPhysicalDeviceProperties &device_properties,
                 VkSurfaceKHR surface,
                 const std::vector<const char *> &required_extensions) -> bool {
  if (device_properties.apiVersion < VK_API_VERSION_1_2) {
    return false;
  }
  auto queue_family_indices = FindQueueFamilies(device, surface);
  auto swap_chain_support = QuerySwapChainSupport(device, surface);
  bool swap_chain_adequate = !swap_chain_support.formats.empty() &&
                             !swap_chain_support.present_modes.empty();
  return queue_family_indices.IsComplete() &&
         CheckDeviceExtensionsSupport(device, required_extensions) &&
         swap_chain_adequate;
}

VulkanContext::VulkanContext(VkInstance instance_,
                             VkSurfaceKHR surface_) noexcept
    : instance(instance_), surface(surface_),
      swap_chain_image_format(VK_FORMAT_UNDEFINED),
      swap_chain_image_extent({}) {
  required_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
  };
}

VulkanContext::VulkanContext(VulkanContext &&other) noexcept
    : is_initialized(other.is_initialized), instance(other.instance),
      surface(other.surface), physical_device(other.physical_device),
      device(other.device), graphics_queue(other.graphics_queue),
      present_queue(other.present_queue), swap_chain(other.swap_chain),
      swap_chain_images(std::move(other.swap_chain_images)),
      swap_chain_image_format(other.swap_chain_image_format),
      swap_chain_image_extent(other.swap_chain_image_extent),
      image_views(std::move(other.image_views)),
      swap_chain_image_ready_to_present_semaphores(
          std::move(other.swap_chain_image_ready_to_present_semaphores)),
      required_extensions(std::move(other.required_extensions)) {
  other.instance = VK_NULL_HANDLE;
  other.surface = VK_NULL_HANDLE;
  other.physical_device = VK_NULL_HANDLE;
  other.device = VK_NULL_HANDLE;
  other.graphics_queue = VK_NULL_HANDLE;
  other.present_queue = VK_NULL_HANDLE;
  other.swap_chain = VK_NULL_HANDLE;
  other.swap_chain_images.clear();
  other.swap_chain_image_format = VK_FORMAT_UNDEFINED;
  other.swap_chain_image_extent = {};
  other.image_views.clear();
  other.swap_chain_image_ready_to_present_semaphores.clear();
  other.required_extensions.clear();
  other.is_initialized = false;
}

auto VulkanContext::operator=(VulkanContext &&other) noexcept
    -> VulkanContext & {
  if (this != &other) {
    instance = other.instance;
    surface = other.surface;
    physical_device = other.physical_device;
    device = other.device;
    graphics_queue = other.graphics_queue;
    present_queue = other.present_queue;
    swap_chain = other.swap_chain;
    swap_chain_images = std::move(other.swap_chain_images);
    swap_chain_image_format = other.swap_chain_image_format;
    swap_chain_image_extent = other.swap_chain_image_extent;
    image_views = std::move(other.image_views);
    swap_chain_image_ready_to_present_semaphores =
        std::move(other.swap_chain_image_ready_to_present_semaphores);
    required_extensions = std::move(other.required_extensions);
    is_initialized = other.is_initialized;
    other.instance = VK_NULL_HANDLE;
    other.surface = VK_NULL_HANDLE;
    other.physical_device = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
    other.graphics_queue = VK_NULL_HANDLE;
    other.present_queue = VK_NULL_HANDLE;
    other.swap_chain = VK_NULL_HANDLE;
    other.swap_chain_images.clear();
    other.swap_chain_image_format = VK_FORMAT_UNDEFINED;
    other.swap_chain_image_extent = {};
    other.image_views.clear();
    other.swap_chain_image_ready_to_present_semaphores.clear();
    other.required_extensions.clear();
    other.is_initialized = false;
  }
  return *this;
}

auto VulkanContext::Initialize() noexcept
    -> std::expected<void, VkInitializationError> {
  if (is_initialized) {
    return std::expected<void, VkInitializationError>{};
  }
  auto select_physical_device_result = SelectPhysicalDevice();
  if (!select_physical_device_result) {
    return std::unexpected(select_physical_device_result.error());
  }
  auto create_logical_device_result = CreateLogicalDevice();
  if (!create_logical_device_result) {
    return std::unexpected(create_logical_device_result.error());
  }
  auto create_swap_chain_result = CreateSwapChain();
  if (!create_swap_chain_result) {
    return std::unexpected(create_swap_chain_result.error());
  }
  auto create_image_views_result = CreateImageViews();
  if (!create_image_views_result) {
    return std::unexpected(create_image_views_result.error());
  }
  for (size_t i = 0; i < swap_chain_images.size(); i++) {
    auto create_semaphore_result = CreateSemaphore();
    if (!create_semaphore_result) {
      return std::unexpected(create_semaphore_result.error());
    }
    swap_chain_image_ready_to_present_semaphores.push_back(
        create_semaphore_result.value());
  }
  is_initialized = true;
  return std::expected<void, VkInitializationError>{};
}

auto VulkanContext::GetDevice() const noexcept -> const VkDevice & {
  return device;
}

auto VulkanContext::GetSwapChain() const noexcept -> const VkSwapchainKHR & {
  return swap_chain;
}

auto VulkanContext::GetSwapChainImage(u32 index) const noexcept
    -> const VkImage & {
  return swap_chain_images[index];
}

auto VulkanContext::GetSwapChainImageFormat() const noexcept
    -> const VkFormat & {
  return swap_chain_image_format;
}

auto VulkanContext::GetSwapChainImageExtent() const noexcept
    -> const VkExtent2D & {
  return swap_chain_image_extent;
}

auto VulkanContext::GetSwapChainImageView(u32 index) const noexcept
    -> const VkImageView & {
  return image_views[index];
}

auto VulkanContext::GetSwapChainImageCount() const noexcept -> u32 {
  return static_cast<u32>(swap_chain_images.size());
}

auto VulkanContext::GetGraphicsQueue() const noexcept -> const VkQueue & {
  return graphics_queue;
}

auto VulkanContext::GetPresentQueue() const noexcept -> const VkQueue & {
  return present_queue;
}

auto VulkanContext::GetSwapChainImageReadyToPresentSemaphore(
    u32 index) const noexcept -> const VkSemaphore & {
  return swap_chain_image_ready_to_present_semaphores[index];
}

auto VulkanContext::ResetContext() noexcept -> void {
  if (!is_initialized) {
    return;
  }
  for (auto &semaphore : swap_chain_image_ready_to_present_semaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, semaphore, nullptr);
      semaphore = VK_NULL_HANDLE;
    }
  }
  swap_chain_image_ready_to_present_semaphores.clear();
  for (auto &image_view : image_views) {
    if (image_view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, image_view, nullptr);
      image_view = VK_NULL_HANDLE;
    }
  }
  image_views.clear();
  if (swap_chain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swap_chain, nullptr);
    swap_chain = VK_NULL_HANDLE;
  }
  if (device != VK_NULL_HANDLE) {
    vkDestroyDevice(device, nullptr);
    device = VK_NULL_HANDLE;
  }
  physical_device = VK_NULL_HANDLE;
  graphics_queue = VK_NULL_HANDLE;
  present_queue = VK_NULL_HANDLE;
  swap_chain_images.clear();
  swap_chain_image_format = VK_FORMAT_UNDEFINED;
  swap_chain_image_extent = {};
  swap_chain_image_ready_to_present_semaphores.clear();
  is_initialized = false;
}

auto VulkanContext::CreateSemaphore() const noexcept
    -> std::expected<VkSemaphore, VkInitializationError> {
  VkSemaphoreCreateInfo semaphore_create_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VkSemaphore semaphore = VK_NULL_HANDLE;
  if (vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphore) !=
      VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create semaphore"});
  }
  return semaphore;
}

auto VulkanContext::CreateFence(bool signaled) const noexcept
    -> std::expected<VkFence, VkInitializationError> {
  VkFenceCreateFlags fence_create_flags = 0;
  if (signaled) {
    fence_create_flags = VK_FENCE_CREATE_SIGNALED_BIT;
  }
  VkFenceCreateInfo fence_create_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = fence_create_flags,
  };
  VkFence fence = VK_NULL_HANDLE;
  if (vkCreateFence(device, &fence_create_info, nullptr, &fence) !=
      VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create fence"});
  }
  return fence;
}

auto VulkanContext::SelectPhysicalDevice() noexcept
    -> std::expected<void, VkInitializationError> {
  u32 device_count = 0;
  vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  if (device_count == 0) {
    return std::unexpected(
        VkInitializationError{.message = "No physical devices found"});
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

  std::multimap<u32, VkPhysicalDevice> candidate_devices;
  for (const auto &device : devices) {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    // first check if the device meets the minimum requirements
    if (IsDeviceSuitable(device, device_properties, surface,
                         required_extensions)) {
      auto device_score = CalculateDeviceScore(device_properties);
      candidate_devices.insert({device_score, device});
    }
  }

  if (candidate_devices.empty()) {
    return std::unexpected(
        VkInitializationError{.message = "No suitable physical device found"});
  }

  physical_device = candidate_devices.rbegin()->second;
  return std::expected<void, VkInitializationError>{};
}

auto VulkanContext::CreateLogicalDevice() noexcept
    -> std::expected<void, VkInitializationError> {
  auto queue_family_indices = FindQueueFamilies(physical_device, surface);

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  std::set<u32> unique_queue_families = {
      queue_family_indices.graphics_family.value(),
      queue_family_indices.present_family.value(),
  };

  float queue_priority = 1.0F;
  for (auto queue_family : unique_queue_families) {
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    queue_create_infos.push_back(queue_create_info);
  }

  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{};
  dynamic_rendering_feature.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
  dynamic_rendering_feature.dynamicRendering = VK_TRUE;

  VkPhysicalDeviceFeatures device_features = {};

  VkDeviceCreateInfo device_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &dynamic_rendering_feature,
      .flags = 0,
      .queueCreateInfoCount = static_cast<u32>(queue_create_infos.size()),
      .pQueueCreateInfos = queue_create_infos.data(),
      .enabledLayerCount = 0,         // legacy and should not be used
      .ppEnabledLayerNames = nullptr, // legacy and should not be used
      .enabledExtensionCount = static_cast<u32>(required_extensions.size()),
      .ppEnabledExtensionNames = required_extensions.data(),
      .pEnabledFeatures = &device_features,
  };

  if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) !=
      VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create logical device"});
  }

  vkGetDeviceQueue(device, queue_family_indices.graphics_family.value(), 0,
                   &graphics_queue);
  vkGetDeviceQueue(device, queue_family_indices.present_family.value(), 0,
                   &present_queue);
  if (graphics_queue == VK_NULL_HANDLE || present_queue == VK_NULL_HANDLE) {
    return std::unexpected(VkInitializationError{
        .message = "Failed to get graphics or present queue"});
  }
  device_queue_family_indices = queue_family_indices;
  return std::expected<void, VkInitializationError>{};
}

auto VulkanContext::CreateSwapChain() noexcept
    -> std::expected<void, VkInitializationError> {
  auto swap_chain_support = QuerySwapChainSupport(physical_device, surface);
  auto surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
  auto present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
  auto extent = ChooseSwapExtent(swap_chain_support.capabilities);

  u32 image_count = swap_chain_support.capabilities.minImageCount + 1;
  if (swap_chain_support.capabilities.maxImageCount > 0 &&
      image_count > swap_chain_support.capabilities.maxImageCount) {
    image_count = swap_chain_support.capabilities.maxImageCount;
  }

  std::array<u32, 2> queue_family_indices_array = {
      device_queue_family_indices.graphics_family.value(),
      device_queue_family_indices.present_family.value(),
  };

  VkSwapchainCreateInfoKHR swap_chain_create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .preTransform = swap_chain_support.capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  if (device_queue_family_indices.graphics_family !=
      device_queue_family_indices.present_family) {
    swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swap_chain_create_info.queueFamilyIndexCount = 2;
    swap_chain_create_info.pQueueFamilyIndices =
        queue_family_indices_array.data();
  } else {
    swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_create_info.queueFamilyIndexCount = 0;
    swap_chain_create_info.pQueueFamilyIndices = nullptr;
  }

  if (vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                           &swap_chain) != VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create swap chain"});
  }

  u32 actual_image_count = 0;
  vkGetSwapchainImagesKHR(device, swap_chain, &actual_image_count, nullptr);
  swap_chain_images.resize(actual_image_count);
  vkGetSwapchainImagesKHR(device, swap_chain, &actual_image_count,
                          swap_chain_images.data());
  swap_chain_image_format = surface_format.format;
  swap_chain_image_extent = extent;

  return std::expected<void, VkInitializationError>{};
}

auto VulkanContext::CreateImageViews() noexcept
    -> std::expected<void, VkInitializationError> {
  image_views.resize(swap_chain_images.size());
  for (size_t i = 0; i < swap_chain_images.size(); i++) {
    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = swap_chain_images[i],

        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swap_chain_image_format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    if (vkCreateImageView(device, &image_view_create_info, nullptr,
                          &image_views[i]) != VK_SUCCESS) {
      return std::unexpected(
          VkInitializationError{.message = "Failed to create image view"});
    }
  }
  return std::expected<void, VkInitializationError>{};
}

auto VulkanContext::CreateCommandPool() noexcept
    -> std::expected<VkCommandPool, VkInitializationError> {
  VkCommandPoolCreateInfo command_pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = device_queue_family_indices.graphics_family.value(),
  };
  VkCommandPool command_pool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(device, &command_pool_create_info, nullptr,
                          &command_pool) != VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to create command pool"});
  }
  return command_pool;
}

auto VulkanContext::CreateCommandBuffer(VkCommandPool command_pool)
    const noexcept -> std::expected<VkCommandBuffer, VkInitializationError> {
  VkCommandBufferAllocateInfo command_buffer_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device, &command_buffer_allocate_info,
                               &command_buffer) != VK_SUCCESS) {
    return std::unexpected(
        VkInitializationError{.message = "Failed to allocate command buffer"});
  }
  return command_buffer;
}

} // namespace lumina::renderer
