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
