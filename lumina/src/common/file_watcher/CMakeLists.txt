# File watcher library. A static library rather than part of the header-only
# lumina_common, because polling a modification time goes through the
# PlatformServices function pointers and lumina_common must stay free of that
# dependency.

# The header is addressed as "file_watcher/file_watcher.hpp", matching the way
# logger/ and data_structures/ headers are reached, so the base directory is
# common/ rather than this directory.
get_filename_component(LUMINA_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)

add_library(lumina_file_watcher STATIC)

target_sources(
  lumina_file_watcher
  PRIVATE file_watcher.cpp
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${LUMINA_COMMON_DIR}
         FILES
         file_watcher.hpp)

target_include_directories(
  lumina_file_watcher
  PUBLIC ${LUMINA_COMMON_DIR}
         $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

target_link_libraries(lumina_file_watcher PUBLIC lumina_common)
target_link_libraries(lumina_file_watcher PRIVATE lumina_platform_common)
