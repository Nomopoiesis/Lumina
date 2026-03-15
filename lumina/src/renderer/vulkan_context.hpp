#pragma once

#include <vulkan/vulkan.h>

#include "lumina_types.hpp"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace lumina::renderer {

class LuminaRenderer;

struct VkInitializationError {
  std::string message;
};

class VulkanContext final {
public:
  struct QueueFamilyIndices {
    std::optional<u32> graphics_family;
    std::optional<u32> present_family;

    [[nodiscard]] auto IsComplete() const noexcept -> bool {
      return graphics_family.has_value() && present_family.has_value();
    }
  };

  VulkanContext() noexcept = default;
  VulkanContext(LuminaRenderer *renderer, VkInstance instance_,
                VkSurfaceKHR surface_) noexcept;
  VulkanContext(VulkanContext &&other) noexcept;
  auto operator=(VulkanContext &&other) noexcept -> VulkanContext &;

  VulkanContext(const VulkanContext &) = delete;
  auto operator=(const VulkanContext &) -> VulkanContext & = delete;

  ~VulkanContext() { ResetContext(); }

  auto Initialize() noexcept -> std::expected<void, VkInitializationError>;
  auto ResetContext() noexcept -> void;

  auto CreateSemaphore() const noexcept
      -> std::expected<VkSemaphore, VkInitializationError>;
  auto CreateFence(bool signaled = false) const noexcept
      -> std::expected<VkFence, VkInitializationError>;

  [[nodiscard]] auto GetDevice() const noexcept -> const VkDevice &;
  [[nodiscard]] auto GetPhysicalDevice() const noexcept
      -> const VkPhysicalDevice &;
  [[nodiscard]] auto GetSwapChain() const noexcept -> const VkSwapchainKHR &;
  [[nodiscard]] auto GetSwapChainImage(u32 index) const noexcept
      -> const VkImage &;
  [[nodiscard]] auto GetSwapChainImageFormat() const noexcept
      -> const VkFormat &;
  [[nodiscard]] auto GetSwapChainImageExtent() const noexcept
      -> const VkExtent2D &;
  [[nodiscard]] auto GetSwapChainImageView(u32 index) const noexcept
      -> const VkImageView &;
  [[nodiscard]] auto GetSwapChainImageCount() const noexcept -> u32;
  [[nodiscard]] auto GetGraphicsQueue() const noexcept -> const VkQueue &;
  [[nodiscard]] auto GetPresentQueue() const noexcept -> const VkQueue &;
  [[nodiscard]] auto
  GetSwapChainImageReadyToPresentSemaphore(u32 index) const noexcept
      -> const VkSemaphore &;

  auto CreateCommandPool() noexcept
      -> std::expected<VkCommandPool, VkInitializationError>;
  auto CreateCommandBuffer(VkCommandPool command_pool) const noexcept
      -> std::expected<VkCommandBuffer, VkInitializationError>;

  auto CreateImageView(VkImage &image, VkFormat format)
      -> std::expected<VkImageView, VkInitializationError>;

  auto RecreateSwapChain() noexcept
      -> std::expected<void, VkInitializationError>;

  [[nodiscard]] auto GetPhysicalDeviceProperties() const noexcept
      -> const VkPhysicalDeviceProperties & {
    return physical_device_properties;
  }

private:
  auto SelectPhysicalDevice() noexcept
      -> std::expected<void, VkInitializationError>;
  auto CreateLogicalDevice() noexcept
      -> std::expected<void, VkInitializationError>;
  auto CreateSwapChain() noexcept -> std::expected<void, VkInitializationError>;
  auto CreateSwapChainImageViews() noexcept
      -> std::expected<void, VkInitializationError>;

  auto DestroySwapChain() noexcept -> void;
  auto DestroySwapChainImageViews() noexcept -> void;

  LuminaRenderer *renderer = nullptr;

  bool is_initialized = false;

  std::vector<const char *> required_extensions;

  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties physical_device_properties;
  VkDevice device = VK_NULL_HANDLE;
  QueueFamilyIndices device_queue_family_indices;
  VkQueue graphics_queue = VK_NULL_HANDLE;
  VkQueue present_queue = VK_NULL_HANDLE;

  VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
  std::vector<VkImage> swap_chain_images;
  VkFormat swap_chain_image_format{};
  VkExtent2D swap_chain_image_extent{};
  std::vector<VkImageView> image_views;
  std::vector<VkSemaphore> swap_chain_image_ready_to_present_semaphores;
};

} // namespace lumina::renderer
