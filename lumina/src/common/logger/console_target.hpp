#pragma once

#include "log_target.hpp"
#include "platform/platform_common/file_handle.hpp"
#include "platform/platform_common/platform_services.hpp"

#include <mutex>
#include <string>

namespace lumina::common::logger {

using lumina::platform::common::InvalidFileHandle;

// Console log target implementation
// Writes log messages to the console using platform-specific console operations
// Thread-safe: uses mutex to protect console writes
class ConsoleTarget : public ILogTarget {
public:
  // Constructs a console target with the given platform services.
  // Creates/attaches to console on initialization.
  // When enable_colors is true, the target will emit ANSI color codes
  // based on the LogLevel passed to Write().
  explicit ConsoleTarget(
      lumina::platform::common::PlatformServices &platform_services,
      bool enable_colors = true);

  ~ConsoleTarget() override = default;

  ConsoleTarget(const ConsoleTarget &) = delete;
  auto operator=(const ConsoleTarget &) -> ConsoleTarget & = delete;
  ConsoleTarget(ConsoleTarget &&) noexcept = delete;
  auto operator=(ConsoleTarget &&) noexcept -> ConsoleTarget & = delete;

  // Writes a formatted log message to the console
  // Thread-safe: protected by mutex
  auto Write(LogLevel level, const std::string &message) -> void override;

  // Optionally wait for a key press at shutdown so that the console window
  // stays open long enough to inspect log output.
  auto OnShutdown(bool wait_for_keypress) -> void override;

  [[nodiscard]] auto IsValid() const -> bool override {
    return console_handle_ != InvalidFileHandle;
  }

private:
  lumina::platform::common::PlatformServices &platform_services_;
  lumina::platform::common::FileHandle console_handle_;
  bool enable_colors_{true};
  std::mutex write_mutex_;
};

} // namespace lumina::common::logger