include(FetchContent)

# Download GLM 1.0.1 from https://github.com/g-truc/glm/releases/tag/1.0.1
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE # Download only the latest commit to save time (no full
                   # history needed)
  SYSTEM # See the note on lumina_stb below - external headers should not report
         # warnings against our warning level.
)
FetchContent_MakeAvailable(glm)

# Download Clay - https://github.com/nicbarker/clay
FetchContent_Declare(
  clay
  GIT_REPOSITORY https://github.com/nicbarker/clay.git
  GIT_TAG v0.14
  GIT_SHALLOW TRUE # Download only the latest commit to save time (no full
                   # history needed)
  SYSTEM # Covers any target clay defines. The path lumina_core actually
         # includes clay.h through is marked SYSTEM at its own call site.
)
# Disable all examples to save space
set(CLAY_INCLUDE_ALL_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(clay)

# external stb_truetype.h
add_library(lumina_stb INTERFACE)

target_sources(
  lumina_stb
  INTERFACE FILE_SET
            HEADERS
            BASE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}
            FILES
            stb_truetype.h
            stb_image.h)

# SYSTEM marks the stb headers as external, which suppresses warnings coming
# from them without touching the warning level of the consuming target.
# Do NOT use `target_compile_options(... INTERFACE -w)` here - that propagates
# to every translation unit of every consumer, silencing our own warnings too.
target_include_directories(lumina_stb SYSTEM
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
