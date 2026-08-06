#pragma once

#include "lumina_assert.hpp"

namespace lumina::common {
// Defined in lumina_terminate.cpp, flushes logger if initialized
auto FlushLoggerIfReady() noexcept -> void;
} // namespace lumina::common

// The flush must happen in every configuration — without it a release build
// terminates silently and loses the log lines explaining why. Only the debug
// break is configuration-dependent (LUMINA_DEBUG_BREAK is already a no-op
// under NDEBUG).
#define LUMINA_TERMINATE()                                                     \
  do {                                                                         \
    lumina::common::FlushLoggerIfReady();                                      \
    LUMINA_DEBUG_BREAK();                                                      \
    std::terminate();                                                          \
  } while (false)
