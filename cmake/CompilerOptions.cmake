# Compiler-specific options and warnings

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
