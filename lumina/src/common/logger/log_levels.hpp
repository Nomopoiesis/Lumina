#pragma once

#include "lumina_types.hpp"

// Windows.h defines ERROR as a macro, which conflicts with our enum value
// Undefine it if it exists
#ifdef ERROR
#undef ERROR
#endif

namespace lumina::common::logger {

enum class LogLevel : u8 {
  TRACE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  CRITICAL,
};

constexpr auto LogLevelToString(LogLevel level) -> const char * {
  switch (level) {
    case LogLevel::TRACE:
      return "TRACE";
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::CRITICAL:
      return "CRITICAL";
  }
}
} // namespace lumina::common::logger
