add_executable(linux_lumina)

target_sources(
  linux_lumina PRIVATE linux_main.cpp linux_window.cpp linux_runtime_root.cpp
                       linux_platform_services.cpp)

target_include_directories(
  linux_lumina PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                      $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

target_link_libraries(
  linux_lumina
  PRIVATE lumina_common
          lumina_logger
          lumina_platform_common
          lumina_path_registry
          lumina_common_terminate
          linux_lumina_vulkan
          lumina_core)

target_link_libraries(linux_lumina PRIVATE ${XCB_LIBRARIES})

add_compile_definitions(VK_USE_PLATFORM_XCB_KHR=1)

add_subdirectory(vulkan)

set_target_properties(linux_lumina PROPERTIES OUTPUT_NAME lumina)

set(LUMINA_BIN_OUT_TARGET
    linux_lumina
    PARENT_SCOPE)
