# Required dependencies

# Vulkan
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
  message(FATAL_ERROR "Vulkan not found")
else()
  message(STATUS "Vulkan found")
endif()
