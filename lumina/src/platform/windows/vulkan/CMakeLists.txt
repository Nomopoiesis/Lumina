add_library(windows_lumina_vulkan STATIC)

target_sources(
  windows_lumina_vulkan PRIVATE win_vulkan.cpp win_vulkan_instance.cpp
                                win_vulkan_surface.cpp)
target_include_directories(
  windows_lumina_vulkan
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
         $<TARGET_PROPERTY:windows_lumina,INTERFACE_INCLUDE_DIRECTORIES>
         $<TARGET_PROPERTY:lumina_platform_dir,LUMINA_PLATFORM_DIR>)

target_link_libraries(
  windows_lumina_vulkan PUBLIC Vulkan::Vulkan lumina_common
                               lumina_platform_common_vulkan)
