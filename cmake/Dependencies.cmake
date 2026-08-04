# Required dependencies

# Vulkan
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
  message(FATAL_ERROR "Vulkan not found")
else()
  message(STATUS "Vulkan found")
endif()

# XCB (linux)
if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(XCB REQUIRED xcb)
  if(NOT XCB_FOUND)
    message(FATAL_ERROR "XCB not found")
  else()
    message(STATUS "XCB found")
  endif()
endif()
