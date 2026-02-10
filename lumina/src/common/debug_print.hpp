#pragma once

#ifdef NDEBUG

#define DBG_PRINT(msg) ((void)0)

#else

#include <print>
#define DBG_PRINT(msg)                                                         \
  std::println("[DEBUG] {}:{} in {}() - {}", __FILE__, __LINE__, __func__, msg)

#endif // NDEBUG
