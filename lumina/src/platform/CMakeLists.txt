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

add_custom_target(lumina_install_runtime_data ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/lumina/data/runtime"
      "$<TARGET_FILE_DIR:${LUMINA_BIN_OUT_TARGET}>"
  COMMENT "Installing runtime data to build output...")

add_dependencies(lumina_install_runtime_data ${LUMINA_BIN_OUT_TARGET})
