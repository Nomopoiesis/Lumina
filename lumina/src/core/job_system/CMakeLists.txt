add_library(lumina_core_job_system STATIC)
target_sources(lumina_core_job_system PRIVATE job_manager.cpp)

target_include_directories(lumina_core_job_system
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lumina_core_job_system PRIVATE lumina_common
                                                     lumina_platform_common)
