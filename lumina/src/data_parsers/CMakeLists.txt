add_library(lumina_data_parsers STATIC)

target_include_directories(
  lumina_data_parsers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                             $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

target_sources(lumina_data_parsers PRIVATE obj_parser.cpp)

target_link_libraries(lumina_data_parsers PRIVATE lumina_common)
