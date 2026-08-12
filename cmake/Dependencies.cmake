# Required dependencies

# Vulkan
#
# The version is stated rather than left implicit because a plain
# find_package(Vulkan REQUIRED) reports "Vulkan found" for any version at all,
# including one whose headers predate the API level the engine asks for. That
# gap is not academic: distribution packages lag the LunarG SDK by a long way,
# so the same source tree can configure cleanly on Windows and then fail on
# Linux at whichever translation unit first names a newer macro. Passing the
# version to find_package moves that to configure time, where the message can
# say which version was found instead of which identifier was undeclared.
#
# Keep this in step with the apiVersion in VkApplicationInfo, set in
# platform_common/vulkan/vulkan_instance.cpp - the headers must be new enough to
# define the VK_API_VERSION_1_x macro that file names.
set(LUMINA_VULKAN_MIN_VERSION 1.3)

find_package(Vulkan ${LUMINA_VULKAN_MIN_VERSION} REQUIRED)
message(STATUS "Vulkan ${Vulkan_VERSION} found (require >= "
               "${LUMINA_VULKAN_MIN_VERSION})")

# string_VkResult, used by VK_CHECK in renderer/vk_check.hpp and by roughly ten
# other translation units, comes from vk_enum_string_helper.h - a generated
# header that upstream keeps in Vulkan-Utility-Libraries rather than in
# Vulkan-Headers. The LunarG SDK bundles both into one include directory, so
# Windows never sees the split; Linux distributions package them separately and
# FindVulkan does not look for the utility half. That combination produces a
# configure step that reports "Vulkan found" and a build that then fails on a
# missing header, which is the surprise worth removing.
#
# Compiling the include rather than probing for a path means this check passes
# in exactly the cases the real build would find the header.
include(CheckIncludeFileCXX)

set(_lumina_saved_required_includes ${CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_INCLUDES ${Vulkan_INCLUDE_DIRS})
check_include_file_cxx(vulkan/vk_enum_string_helper.h
                       LUMINA_HAVE_VK_ENUM_STRING_HELPER)
set(CMAKE_REQUIRED_INCLUDES ${_lumina_saved_required_includes})
unset(_lumina_saved_required_includes)

if(NOT LUMINA_HAVE_VK_ENUM_STRING_HELPER)
  message(
    FATAL_ERROR
      "vulkan/vk_enum_string_helper.h was not found in the Vulkan include "
      "directories (${Vulkan_INCLUDE_DIRS}), but renderer/vk_check.hpp needs "
      "it for string_VkResult.\n"
      "It ships with Vulkan-Utility-Libraries, packaged separately from the "
      "Vulkan headers themselves:\n"
      "  Debian/Ubuntu: vulkan-utility-libraries-dev\n"
      "  Fedora:        vulkan-utility-libraries-devel\n"
      "  Arch:          vulkan-utility-libraries\n"
      "  Windows/macOS: install the LunarG Vulkan SDK, which bundles it, and "
      "set the VULKAN_SDK environment variable.\n"
      "Prefer the version matching the installed Vulkan headers so the "
      "generated enum strings agree with the enum values.")
endif()
message(STATUS "Vulkan utility headers found")

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
