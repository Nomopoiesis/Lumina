#pragma once

#include "log_levels.hpp"

#include <string>

namespace lumina::common::logger {

// Abstract base class for log targets
// Implementations (e.g., ConsoleTarget, FileTarget) will provide concrete
// Write() methods to output log messages to their respective destinations
class ILogTarget {
public:
  ILogTarget() noexcept = default;
  virtual ~ILogTarget() = default;

  ILogTarget(const ILogTarget &) = delete;
  auto operator=(const ILogTarget &) -> ILogTarget & = delete;
  ILogTarget(ILogTarget &&) noexcept = default;
  auto operator=(ILogTarget &&) noexcept -> ILogTarget & = default;

  // Writes a formatted log message with its level to the target.
  // The message is already formatted and ready to be written.
  // This method should be thread-safe (implementations will use mutexes if
  // needed).
  virtual auto Write(LogLevel level, const std::string &message) -> void = 0;

  // Optional hook invoked when the logger is shutting down.
  // Default implementation does nothing; specific targets may override to
  // perform any final actions (e.g., keeping the console open until a key
  // is pressed).
  virtual auto OnShutdown(bool /*wait_for_keypress*/) -> void {}
};

} // namespace lumina::common::logger
