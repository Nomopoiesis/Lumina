add_library(lumina_core STATIC)

target_sources(lumina_core PRIVATE camera.cpp lumina_engine.cpp)

target_include_directories(
  lumina_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                     $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

add_subdirectory(job_system)

target_link_libraries(lumina_core PRIVATE lumina_common lumina_renderer
                                          lumina_core_job_system)
