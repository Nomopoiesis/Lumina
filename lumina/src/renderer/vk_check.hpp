#pragma once

#include "common/logger/logger.hpp"
#include "common/lumina_terminate.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

// Wraps a Vulkan call that returns VkResult.
//
// Unlike ASSERT, the expression is evaluated exactly once in every
// configuration. ASSERT expands to ((void)0) under NDEBUG, which deletes the
// wrapped call from release builds entirely, leaving the output handles
// uninitialized — never put a Vulkan call inside an ASSERT.
#define VK_CHECK(expression, message)                                          \
  do {                                                                         \
    const VkResult vk_check_result = (expression);                             \
    if (vk_check_result != VK_SUCCESS) {                                       \
      LOG_CRITICAL("{} ({})", (message), string_VkResult(vk_check_result));     \
      LUMINA_TERMINATE();                                                      \
    }                                                                          \
  } while (false)
