include(FetchContent)

# SPIRV-Reflect (source compiled directly into the tool, no cmake targets
# needed)
set(SPIRV_REFLECT_EXECUTABLE
    OFF
    CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  spirv_reflect
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
  GIT_TAG main
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(spirv_reflect)

# CppGen - check for and fetch new versions on each configure (only for cppgen)
set(CPPGEN_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)

FetchContent_Declare(
  cppgen
  GIT_REPOSITORY https://github.com/Nomopoiesis/CppGen.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  # SYSTEM marks CppGen's interface include directories external, so its headers
  # do not report warnings against this tool's warning level.
  SYSTEM)
FetchContent_MakeAvailable(cppgen)

add_executable(spirv_reflect_tool src/main.cpp
                                  ${spirv_reflect_SOURCE_DIR}/spirv_reflect.c)

# spirv_reflect.c is vendored source we compile ourselves, so a SYSTEM include
# directory cannot cover it - that only affects headers. Silence the file
# outright; warnings in upstream code we do not edit are not actionable.
set_source_files_properties(${spirv_reflect_SOURCE_DIR}/spirv_reflect.c
                            PROPERTIES COMPILE_OPTIONS -w)

target_include_directories(spirv_reflect_tool
                           PRIVATE ${LUMINA_COMMON_INCLUDE_DIR})

# SYSTEM for spirv_reflect.h, same rationale as lumina_stb in
# third_party/CMakeLists.txt.
target_include_directories(spirv_reflect_tool SYSTEM
                           PRIVATE ${spirv_reflect_SOURCE_DIR})

target_link_libraries(spirv_reflect_tool PRIVATE CppGen)

target_compile_features(spirv_reflect_tool PRIVATE cxx_std_23)

# Place the tool binary alongside the engine binaries
set_target_properties(spirv_reflect_tool PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                    ${CMAKE_BINARY_DIR}/bin)
