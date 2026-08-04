add_library(linux_lumina_vulkan STATIC)

target_sources(linux_lumina_vulkan PRIVATE linux_vulkan.cpp
                                           linux_vulkan_surface.cpp)
target_include_directories(
  linux_lumina_vulkan
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
         $<TARGET_PROPERTY:linux_lumina,INTERFACE_INCLUDE_DIRECTORIES>
         $<TARGET_PROPERTY:lumina_platform_dir,LUMINA_PLATFORM_DIR>)

target_link_libraries(linux_lumina_vulkan PUBLIC Vulkan::Vulkan lumina_common
                                                 lumina_platform_common_vulkan)
