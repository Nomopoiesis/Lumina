include(FetchContent)

# Download GLM 1.0.1 from https://github.com/g-truc/glm/releases/tag/1.0.1
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE # Download only the latest commit to save time (no full
                   # history needed)
)

FetchContent_MakeAvailable(glm)

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

target_compile_options(lumina_stb INTERFACE -w)

target_include_directories(lumina_stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
