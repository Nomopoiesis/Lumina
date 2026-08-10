#pragma once

#include "debug_print.hpp"

// Debug assertion
#ifndef NDEBUG
#define ASSERT(Expression, ...)                                                \
  do {                                                                         \
    if (!(Expression)) {                                                       \
      DBG_PRINT(__VA_ARGS__);                                                  \
      volatile int *crash = nullptr;                                           \
      *crash = 0;                                                              \
    }                                                                          \
  } while (0)
#else
// sizeof leaves Expression unevaluated - no code is generated - while still
// counting as a use of whatever it names. Without that, every variable read
// only by an ASSERT becomes unused as soon as NDEBUG is defined, and Release
// fills up with -Wunused-variable on code that is correct.
#define ASSERT(Expression, ...) ((void)sizeof((Expression)))
#endif

// Debug break
#ifdef NDEBUG
#define LUMINA_DEBUG_BREAK() ((void)0)
#else
#if defined(_MSC_VER)
#define LUMINA_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
#define LUMINA_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(__GNUC__)
#define LUMINA_DEBUG_BREAK() __builtin_trap()
#else
#define LUMINA_DEBUG_BREAK() ((void)0)
#endif
#endif
