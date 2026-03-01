#pragma once

#include "lumina_assert.hpp"

namespace lumina::common {
// Defined in lumina_terminate.cpp, flushes logger if initialized
auto FlushLoggerIfReady() noexcept -> void;
} // namespace lumina::common

#ifdef NDEBUG
#define LUMINA_TERMINATE() std::terminate()
#else
#define LUMINA_TERMINATE()                                                     \
  do {                                                                         \
    lumina::common::FlushLoggerIfReady();                                      \
    LUMINA_DEBUG_BREAK();                                                      \
    std::terminate();                                                          \
  } while (false)
#endif
