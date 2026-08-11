add_library(lumina_core STATIC)

# Use precompiled header to reduce compile times for commonly included headers
target_precompile_headers(lumina_core PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common/pch.hpp>
)

# Core
target_sources(
  lumina_core
  PRIVATE camera_movement_controller.cpp
          entity_manager.cpp
          lumina_engine.cpp
          world.cpp
          static_mesh.cpp
          font.cpp
          mesh_cache.cpp
          ui/ui_system.cpp
          vertex_attribute.cpp
          bounding_box.cpp
          frustum.cpp
          culling.cpp
          drawable_proxy_manager.cpp
          debug_overlay.cpp
          debug_overlay_controller.cpp
          static_mesh_manager.cpp)

# Uniform interface
target_sources(lumina_core PRIVATE uniform_interface/uniform_interface.cpp)

# Job system
target_sources(lumina_core PRIVATE job_system/job_manager.cpp)

# Input
target_sources(lumina_core PRIVATE input/input_state.cpp input/input_action.cpp
                                   input/input_dispatcher.cpp)

target_include_directories(
  lumina_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
         $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>
         ${CMAKE_SOURCE_DIR}/lumina/data/static/shaders/interfaces)

# SYSTEM, for the same reason lumina_stb uses it: clay.h is a single-header
# library we do not maintain, and at our warning level it drowns out our own
# diagnostics. Marking the directory external silences it without lowering the
# warning level of any translation unit that includes it.
target_include_directories(lumina_core SYSTEM PUBLIC ${clay_SOURCE_DIR})

target_link_libraries(
  lumina_core PRIVATE lumina_common lumina_platform_common lumina_renderer
                      lumina_common_terminate lumina_data_parsers lumina_stb)

# PUBLIC because lumina_engine.hpp holds the shader watchers by value, so
# anything including it needs the file watcher header on its path.
target_link_libraries(lumina_core PUBLIC lumina_file_watcher)

# PUBLIC because lumina_engine.hpp holds a FrameProfile by value, so anything
# including it needs the profiler header on its path.
target_link_libraries(lumina_core PUBLIC lumina_profiling)
