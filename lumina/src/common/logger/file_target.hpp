#pragma once

#include "log_target.hpp"
#include "platform/platform_common/file_handle.hpp"
#include "platform/platform_common/platform_services.hpp"

#include <mutex>
#include <string>

namespace lumina::common::logger {

using lumina::platform::common::InvalidFileHandle;

// File log target implementation
// Writes log messages to a file using platform-specific file operations
// Thread-safe: uses mutex to protect file writes
class FileTarget : public ILogTarget {
public:
  // Constructs a file target with the given platform services and file path
  // Opens the file in append mode on initialization
  explicit FileTarget(
      lumina::platform::common::PlatformServices &platform_services,
      const std::string &file_path);

  ~FileTarget() override;

  FileTarget(const FileTarget &) = delete;
  auto operator=(const FileTarget &) -> FileTarget & = delete;
  FileTarget(FileTarget &&) noexcept = delete;
  auto operator=(FileTarget &&) noexcept -> FileTarget & = delete;

  // Writes a formatted log message to the file
  // Thread-safe: protected by mutex
  auto Write(LogLevel level, const std::string &message) -> void override;

  [[nodiscard]] auto IsValid() const -> bool override {
    return file_handle_ != InvalidFileHandle;
  }

private:
  lumina::platform::common::PlatformServices &platform_services_;
  lumina::platform::common::FileHandle file_handle_;
  std::mutex write_mutex_;
};

} // namespace lumina::common::logger