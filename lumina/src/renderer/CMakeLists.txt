add_library(lumina_renderer STATIC)

target_include_directories(
  lumina_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                         $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

target_sources(
  lumina_renderer
  PRIVATE renderer.cpp
          vulkan_context.cpp
          frame_context.cpp
          command_context.cpp
          vertex_layout.cpp
          vertex_serializer.cpp
          shaders/shader_module_cache.cpp)

target_link_libraries(lumina_renderer
                      PRIVATE lumina_common lumina_common_terminate lumina_stb)
target_link_libraries(lumina_renderer PUBLIC Vulkan::Vulkan glm::glm)
