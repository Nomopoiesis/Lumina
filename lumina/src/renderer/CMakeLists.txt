add_library(lumina_renderer STATIC)

# Use precompiled header to reduce compile times for commonly included headers
target_precompile_headers(
  lumina_renderer PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/pch.hpp>)

target_include_directories(
  lumina_renderer
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
         $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>
         ${CMAKE_SOURCE_DIR}/lumina/data/static/shaders/interfaces)

target_sources(
  lumina_renderer
  PRIVATE renderer.cpp
          vulkan_context.cpp
          frame_context.cpp
          command_context.cpp
          vertex_layout.cpp
          graphics_pipeline.cpp
          material_instance.cpp
          material_template.cpp
          vertex_serializer.cpp
          draw_item_registry.cpp
          device_retirement_queue.cpp
          ui_renderer.cpp
          shaders/shader_module_cache.cpp
          shaders/shader_interface.cpp
          shaders/shader_gen/static_shader_api.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/../core/vertex_attribute.cpp)

target_link_libraries(
  lumina_renderer PRIVATE lumina_common lumina_common_terminate lumina_stb
                          lumina_path_registry lumina_profiling)
target_link_libraries(lumina_renderer PUBLIC Vulkan::Vulkan glm::glm)
