add_library(lumina_platform_common_vulkan STATIC)
target_sources(lumina_platform_common_vulkan PRIVATE vulkan_debug_callback.cpp)
target_include_directories(lumina_platform_common_vulkan
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lumina_platform_common_vulkan
                      PRIVATE Vulkan::Vulkan lumina_common lumina_logger)
