# Platform common library - provides platform-agnostic interfaces
add_library(lumina_platform_common STATIC)

target_sources(
  lumina_platform_common
  PRIVATE platform_services.cpp
  PUBLIC FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES
         platform_services.hpp
         runtime_root.hpp)

target_include_directories(
  lumina_platform_common
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
         $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

add_subdirectory(vulkan)
