# Source directory - build modules and executable

add_custom_target(lumina_src_dir)

set_target_properties(lumina_src_dir PROPERTIES LUMINA_SRC_DIR
                                                "${CMAKE_CURRENT_SOURCE_DIR}")

# Add common headers library
add_subdirectory(common)

# Add data parsers module
add_subdirectory(data_parsers)

# Add core module
add_subdirectory(core)

# Add renderer module
add_subdirectory(renderer)

# Add platform module (will add main entry point to executable)
add_subdirectory(platform)
