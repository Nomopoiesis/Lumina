#pragma once

#include "debug_print.hpp"

// Debug assertion
#ifndef NDEBUG
#define ASSERT(Expression, ...)                                                \
  if (!(Expression)) {                                                         \
    DBG_PRINT(__VA_ARGS__);                                                    \
    volatile int *crash = nullptr;                                             \
    *crash = 0;                                                                \
  }
#else
#define ASSERT(Expression)
#endif

// Debug break
#ifdef NDEBUG
#define LUMINA_DEBUG_BREAK() ((void)0)
#else
#if defined(_MSC_VER) || defined(__clang__)
#define LUMINA_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__)
#define LUMINA_DEBUG_BREAK() __builtin_trap()
#else
#define LUMINA_DEBUG_BREAK() ((void)0)
#endif
#endif
