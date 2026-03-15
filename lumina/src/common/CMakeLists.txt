# Common headers library - INTERFACE library for header-only code

add_library(lumina_common INTERFACE)

target_sources(
  lumina_common
  INTERFACE FILE_SET
            HEADERS
            BASE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}
            FILES
            debug_print.hpp
            lumina_assert.hpp
            lumina_types.hpp
            lumina_util.hpp
            scope_guard.hpp
            logger/log_levels.hpp
            logger/log_target.hpp
            logger/console_target.hpp
            logger/file_target.hpp
            logger/logger.hpp
            lumina_terminate.hpp
            data_structures/lock_free_pool.hpp
            data_structures/work_stealing_deque.hpp)

add_library(lumina_common_terminate STATIC)
target_sources(lumina_common_terminate PRIVATE lumina_terminate.cpp)

target_include_directories(lumina_common_terminate
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lumina_common_terminate PRIVATE lumina_logger)

# Logger implementation library - STATIC library for logger source files
add_library(lumina_logger STATIC)

target_sources(
  lumina_logger PRIVATE logger/logger.cpp logger/console_target.cpp
                        logger/file_target.cpp)

target_include_directories(
  lumina_logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                       $<TARGET_PROPERTY:lumina_src_dir,LUMINA_SRC_DIR>)

target_link_libraries(lumina_logger PRIVATE lumina_common
                                            lumina_platform_common)
