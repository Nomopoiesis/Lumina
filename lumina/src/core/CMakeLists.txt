add_library(lumina_core STATIC)

# Core
target_sources(
  lumina_core PRIVATE camera_movement_controller.cpp entity_manager.cpp
                      lumina_engine.cpp world.cpp static_mesh.cpp
                      mesh_cache.cpp)

# Job system
target_sources(lumina_core PRIVATE job_system/job_manager.cpp)

# Input
target_sources(lumina_core PRIVATE input/input_state.cpp input/input_action.cpp
                                   input/input_dispatcher.cpp)

target_include_directories(
  lumina_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                     $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

target_link_libraries(
  lumina_core PRIVATE lumina_common lumina_platform_common lumina_renderer
                      lumina_common_terminate lumina_data_parsers)
