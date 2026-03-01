add_executable(windows_lumina)

target_sources(windows_lumina PRIVATE win_main.cpp win_window.cpp
                                      win_platform_services.cpp)

target_include_directories(
  windows_lumina PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                        $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

# Windows-specific settings Set subsystem to Windows (not console)
set_target_properties(windows_lumina PROPERTIES WIN32_EXECUTABLE TRUE)
# Output name (CMake will automatically add .exe on Windows)
set_target_properties(windows_lumina PROPERTIES OUTPUT_NAME lumina)

target_compile_definitions(
  windows_lumina
  PRIVATE NOMINMAX # prevents min/max macro pollution from minwindef.h
          WIN32_LEAN_AND_MEAN # reduces Windows.h include size, optional but
                              # good practice
)

add_subdirectory(vulkan)

target_link_libraries(
  windows_lumina
  PRIVATE lumina_common lumina_logger lumina_platform_common
          lumina_common_terminate windows_lumina_vulkan lumina_core)

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
          xinput)
