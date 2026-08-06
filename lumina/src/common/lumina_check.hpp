#pragma once

#include "logger/logger.hpp"
#include "lumina_terminate.hpp"

// Fatal precondition check, active in every build configuration.
//
// Use this — not ASSERT — whenever failure leaves the program with no valid way
// to continue: failed resource creation, a missing required asset, or an
// invariant that the following lines dereference. ASSERT expands to ((void)0)
// under NDEBUG and discards its argument entirely, so in a release build the
// condition is neither checked nor evaluated and execution walks straight into
// undefined behaviour.
//
// ASSERT remains appropriate for checks that are purely diagnostic and safe to
// skip in an optimized build. In either macro the condition must be free of
// side effects.
#define LUMINA_CHECK(condition, message)                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      LOG_CRITICAL("{}", (message));                                           \
      LUMINA_TERMINATE();                                                      \
    }                                                                          \
  } while (false)
