#pragma once

#include "log_levels.hpp"
#include "log_target.hpp"
#include "lumina_types.hpp"
#include "platform/common/platform_services.hpp"

#include <atomic>
#include <condition_variable>
#include <format>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace lumina::common::logger {

// Enum for log target types
enum class LogTargetType : u8 {
  Console,
  File,
  Both,
};

// Main logger class with singleton pattern
// Provides thread-safe, asynchronous logging with multiple targets
class Logger {
public:
  Logger(const Logger &) = delete;
  auto operator=(const Logger &) -> Logger & = delete;
  Logger(Logger &&) noexcept = delete;
  auto operator=(Logger &&) noexcept -> Logger & = delete;

  // Get the singleton instance
  static auto Instance() -> Logger & {
    auto &instance = GetStaticInstance();
    if (!instance.is_initialized_) {
      throw std::runtime_error(
          "Logger not initialized, call Initialize() first");
    }
    return instance;
  }

  // Initialize the logger with targets and platform services
  // Must be called before using the logger
  // wait_for_keypress_on_shutdown:
  //   When true and a console target is active, the logger will wait for a
  //   key press during Shutdown(), typically at process exit, so that console
  //   logs remain visible.
  // Core initialization overload with explicit file path, shutdown behavior,
  // and console color control.
  static auto Initialize(
      LogTargetType target_type,
      lumina::platform::common::PlatformServices &platform_services,
      const std::string &file_path,
      bool wait_for_keypress_on_shutdown,
      bool enable_console_colors) -> void;

  // Backwards-compatible overload that defaults console colors to enabled.
  static auto Initialize(
      LogTargetType target_type,
      lumina::platform::common::PlatformServices &platform_services,
      const std::string &file_path,
      bool wait_for_keypress_on_shutdown = false) -> void;

  // Overload for default file path, no keypress wait
  static auto Initialize(
      LogTargetType target_type,
      lumina::platform::common::PlatformServices &platform_services) -> void {
    Initialize(target_type, platform_services, std::string{}, false,
               /*enable_console_colors*/ true);
  }

  // Overload for default file path with explicit keypress wait flag
  static auto Initialize(
      LogTargetType target_type,
      lumina::platform::common::PlatformServices &platform_services,
      bool wait_for_keypress_on_shutdown) -> void {
    Initialize(target_type, platform_services, std::string{},
               wait_for_keypress_on_shutdown,
               /*enable_console_colors*/ true);
  }

  // Overload for default file path with explicit keypress and color flags.
  static auto Initialize(
      LogTargetType target_type,
      lumina::platform::common::PlatformServices &platform_services,
      bool wait_for_keypress_on_shutdown, bool enable_console_colors) -> void {
    Initialize(target_type, platform_services, std::string{},
               wait_for_keypress_on_shutdown, enable_console_colors);
  }

  // Log a message with the given level, file, and line
  // Supports format string with arguments (using std::format syntax)
  template <typename... Args>
  auto Log(LogLevel level, const char *file, u32 line,
           std::format_string<Args...> fmt, Args &&...args) -> void {
    if (!is_initialized_) {
      return; // Logger not initialized, silently ignore
    }

    // Format the message using std::format
    std::string formatted_message;
    if constexpr (sizeof...(args) == 0) {
      formatted_message = std::string(fmt.get());
    } else {
      formatted_message = std::format(fmt, std::forward<Args>(args)...);
    }

    // Create log message
    LogMessage log_msg;
    log_msg.level = level;
    log_msg.file = file;
    log_msg.line = line;
    log_msg.message = std::move(formatted_message);

    // Enqueue message (thread-safe)
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      message_queue_.push(std::move(log_msg));
    }
    queue_condition_.notify_one();
  }

  // Shutdown the logger, flush all messages, and stop background thread
  static auto Shutdown() -> void;

private:
  Logger() noexcept = default;
  ~Logger();

  // Get the static instance
  static auto GetStaticInstance() -> Logger & {
    static Logger instance;
    return instance;
  }

  // Background worker thread function
  auto WorkerThread() -> void;

  // Format a log message with timestamp, level, thread ID, file:line, and
  // message
  auto FormatMessage(LogLevel level, const char *file, u32 line,
                     const std::string &message) -> std::string;

  // Get current timestamp as string
  auto GetTimestamp() -> std::string;

  // Get current thread ID as string
  auto GetThreadId() -> std::string;

  // Message structure for the queue
  struct LogMessage {
    LogLevel level = LogLevel::INFO;
    std::string file;
    u32 line = 0;
    std::string message;
  };

  // Logger state
  std::vector<std::unique_ptr<ILogTarget>> targets_;
  lumina::platform::common::PlatformServices *platform_services_ = nullptr;

  // Thread synchronization
  std::queue<LogMessage> message_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_condition_;
  std::atomic<bool> should_stop_{false};
  std::thread worker_thread_;

  bool wait_for_keypress_on_shutdown_{false};
  bool is_initialized_{false};
  std::mutex init_mutex_;
};

} // namespace lumina::common::logger

// Convenience macros for logging (similar to DBG_PRINT pattern)
// Usage: LOG_INFO("message"), LOG_ERROR("error: {}", error_code), etc.
#define LOG_TRACE(...)                                                         \
  lumina::common::logger::Logger::Instance().Log(                              \
      lumina::common::logger::LogLevel::TRACE, __FILE__, __LINE__,             \
      __VA_ARGS__)

#define LOG_DEBUG(...)                                                         \
  lumina::common::logger::Logger::Instance().Log(                              \
      lumina::common::logger::LogLevel::DEBUG, __FILE__, __LINE__,             \
      __VA_ARGS__)

#define LOG_INFO(...)                                                          \
  lumina::common::logger::Logger::Instance().Log(                              \
      lumina::common::logger::LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARNING(...)                                                       \
  lumina::common::logger::Logger::Instance().Log(                              \
      lumina::common::logger::LogLevel::WARNING, __FILE__, __LINE__,           \
      __VA_ARGS__)

#define LOG_ERROR(...)                                                         \
  lumina::common::logger::Logger::Instance().Log(                              \
      lumina::common::logger::LogLevel::ERROR, __FILE__, __LINE__,             \
      __VA_ARGS__)

#define LOG_CRITICAL(...)                                                      \
  lumina::common::logger::Logger::Instance().Log(                              \
      lumina::common::logger::LogLevel::CRITICAL, __FILE__, __LINE__,          \
      __VA_ARGS__)
