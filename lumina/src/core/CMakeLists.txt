add_library(lumina_core STATIC)

target_sources(lumina_core PRIVATE lumina_engine.cpp)

target_include_directories(
  lumina_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                     $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

target_link_libraries(lumina_core PRIVATE lumina_common lumina_renderer)
