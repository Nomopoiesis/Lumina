#pragma once

// Forward declarations for the Vulkan handle types used in renderer headers,
// so those headers do not have to drag in <vulkan/vulkan.h>. Include the real
// header in .cpp files, and in any header that needs a Vulkan enum, struct or
// function — none of those can be forward declared.
//
// Redeclaring these typedefs is safe: a translation unit that also includes
// <vulkan/vulkan.h> just sees an identical typedef, which C++ permits.

// vulkan_core.h spells non-dispatchable handles as `struct X_T *` only when
// VK_USE_64_BIT_PTR_DEFINES == 1; otherwise they are uint64_t and the typedefs
// below would silently disagree with the real header. The SDK derives that
// macro from the target's pointer width, so assert both rather than let the
// mismatch through.
#if defined(VK_USE_64_BIT_PTR_DEFINES) && (VK_USE_64_BIT_PTR_DEFINES != 1)
#error                                                                         \
    "vulkan_fwd.hpp declares 64-bit Vulkan handles; VK_USE_64_BIT_PTR_DEFINES is 0"
#endif
static_assert(sizeof(void *) == 8,
              "vulkan_fwd.hpp declares Vulkan handles as pointers, which "
              "matches vulkan_core.h only on 64-bit targets");

// Dispatchable handles.
typedef struct VkCommandBuffer_T *VkCommandBuffer;
typedef struct VkDevice_T *VkDevice;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;

// Non-dispatchable handles.
typedef struct VkBuffer_T *VkBuffer;
typedef struct VkDescriptorPool_T *VkDescriptorPool;
typedef struct VkDescriptorSet_T *VkDescriptorSet;
typedef struct VkDescriptorSetLayout_T *VkDescriptorSetLayout;
typedef struct VkDeviceMemory_T *VkDeviceMemory;
typedef struct VkFence_T *VkFence;
typedef struct VkImage_T *VkImage;
typedef struct VkImageView_T *VkImageView;
typedef struct VkPipeline_T *VkPipeline;
typedef struct VkPipelineLayout_T *VkPipelineLayout;
typedef struct VkSampler_T *VkSampler;
typedef struct VkSemaphore_T *VkSemaphore;
typedef struct VkShaderModule_T *VkShaderModule;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
typedef struct VkSwapchainKHR_T *VkSwapchainKHR;

// Matches what vulkan_core.h defines for a C++11-or-later 64-bit target. Its
// own definition is #ifndef-guarded, so whichever header lands first wins and
// both spell it the same way.
#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE nullptr
#endif
