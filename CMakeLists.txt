cmake_minimum_required(VERSION 4.1.1)

# Set C++ standard to C++23 for full module support
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(
  Lumina
  VERSION 0.1.0
  DESCRIPTION "Lumina Engine"
  LANGUAGES CXX)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE STRING "Build type" FORCE)
endif()

# Export compile commands for tools
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE BOOL "Export compile commands" FORCE)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform detection
if(WIN32)
  add_definitions(-D_PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
  add_definitions(-D_PLATFORM_LINUX)
elseif(APPLE)
  add_definitions(-D_PLATFORM_MACOS)
endif()

# Include compiler options
include(cmake/CompilerOptions.cmake)

# Include all required dependencies
include(cmake/Dependencies.cmake)

# Add third-party libraries
add_subdirectory(third_party)

# Add subdirectories
add_subdirectory(lumina)
