# Platform module - delegate to platform-specific subdirectories

add_custom_target(lumina_platform_dir)

set_target_properties(
  lumina_platform_dir PROPERTIES LUMINA_PLATFORM_DIR
                                 "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(common)

# Add platform-specific implementations
if(WIN32)
  add_subdirectory(windows)
elseif(UNIX AND NOT APPLE)
  message(FATAL_ERROR "Linux is not supported yet")
  add_subdirectory(linux)
elseif(APPLE)
  message(FATAL_ERROR "macOS is not supported yet")
  add_subdirectory(macos)
endif()
