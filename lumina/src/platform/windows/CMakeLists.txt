add_executable(windows_lumina)

target_sources(
  windows_lumina PRIVATE win_main.cpp win_window.cpp win_platform_services.cpp
                         win_runtime_root.cpp)

target_include_directories(
  windows_lumina PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                        $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

# Directory scope rather than target scope: the vulkan/ subdirectory builds
# windows_lumina_vulkan, which includes <Windows.h> (directly, and again via
# <vulkan/vulkan.h> once VK_USE_PLATFORM_WIN32_KHR is set). As target_compile_
# definitions on windows_lumina these never reached that target, so it was
# compiling the full unstripped Windows.h and inheriting the min/max macros.
add_compile_definitions(
  VK_USE_PLATFORM_WIN32_KHR=1
  NOMINMAX # prevents min/max macro pollution from minwindef.h
  WIN32_LEAN_AND_MEAN # reduces Windows.h include size
)

# Windows-specific settings Set subsystem to Windows (not console)
set_target_properties(windows_lumina PROPERTIES WIN32_EXECUTABLE TRUE)
# Output name (CMake will automatically add .exe on Windows)
set_target_properties(windows_lumina PROPERTIES OUTPUT_NAME lumina)
set(LUMINA_BIN_OUT_TARGET
    windows_lumina
    PARENT_SCOPE)

target_precompile_headers(
  windows_lumina PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/pch.hpp>)

add_subdirectory(vulkan)

target_link_libraries(
  windows_lumina
  PRIVATE lumina_common
          lumina_logger
          lumina_platform_common
          lumina_common_terminate
          windows_lumina_vulkan
          lumina_core
          lumina_path_registry)

target_link_libraries(
  windows_lumina
  PRIVATE kernel32
          user32
          gdi32
          winspool
          shell32
          ole32
          oleaut32
          uuid
          comdlg32
          advapi32
          winmm
          xinput
          Shlwapi)
