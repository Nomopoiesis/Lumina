# Compiler-specific options and warnings

# CPU zone profiling, on for every configuration that exists today. Performance
# is measured in Release, so a Debug-only profiler — the ASSERT pattern — would
# instrument the build nobody profiles. Written as a per-config expression so a
# future shipping configuration switches it off by name rather than by editing
# call sites.
add_compile_definitions(
  "LUMINA_PROFILING_ENABLED=$<IF:$<CONFIG:Shipping>,0,1>")

# Emit a PDB for every configuration, Release included. Optimizations and NDEBUG
# stay exactly as they were, so release-only bugs still reproduce; the symbols
# just make the resulting crash readable in a debugger.
if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
  add_link_options("$<$<NOT:$<CONFIG:Debug>>:/DEBUG>"
                   "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
                   "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  message(STATUS "Using MSVC compiler")
  # MSVC compiler options
  add_compile_options(
    /W4 # Warning level 4
    /WX- # Don't treat warnings as errors
    /permissive- # Conformance mode
    /Zc:__cplusplus # Enable __cplusplus macro
    /Zc:preprocessor # Enable conforming preprocessor
  )

  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "Using GCC/Clang compiler")
  # GCC/Clang compiler options
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-extra-semi-stmt
    -Wno-switch-default
    -Wno-gnu-anonymous-struct
    -Wno-unsafe-buffer-usage-in-libc-call
    -Wno-nested-anon-types
    -Wno-c++17-extensions
    -Wno-c++20-extensions
    -Wno-c++20-compat
    -Wno-unsafe-buffer-usage)

endif()
