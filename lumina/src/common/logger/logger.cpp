#include "logger.hpp"

#include "console_target.hpp"
#include "file_target.hpp"

#include <chrono>
#include <sstream>
#include <thread>

namespace lumina::common::logger {

auto Logger::Initialize(
    LogTargetType target_type,
    lumina::platform::common::PlatformServices &platform_services,
    const std::string &file_path, bool wait_for_keypress_on_shutdown,
    bool enable_console_colors) -> void {
  auto &logger = GetStaticInstance();
  std::lock_guard<std::mutex> lock(logger.init_mutex_);

  if (logger.is_initialized_) {
    std::print(
        "Logger already initialized, initializing logger multiple times is not "
        "allowed, Consider Shutdown() first");
    std::terminate();
  }

  logger.platform_services_ = &platform_services;
  logger.wait_for_keypress_on_shutdown_ = wait_for_keypress_on_shutdown;

  // Create targets based on target_type
  if (target_type == LogTargetType::Console ||
      target_type == LogTargetType::Both) {
    logger.targets_.push_back(std::make_unique<ConsoleTarget>(
        platform_services, enable_console_colors));
  }

  if (target_type == LogTargetType::File ||
      target_type == LogTargetType::Both) {
    if (file_path.empty()) {
      // Default file path if not provided
      logger.targets_.push_back(
          std::make_unique<FileTarget>(platform_services, "lumina.log"));
    } else {
      logger.targets_.push_back(
          std::make_unique<FileTarget>(platform_services, file_path));
    }
  }

  // Start background worker thread
  logger.should_stop_ = false;
  logger.worker_thread_ = std::thread(&Logger::WorkerThread, &logger);

  logger.is_initialized_ = true;
}

auto Logger::Initialize(
    LogTargetType target_type,
    lumina::platform::common::PlatformServices &platform_services,
    const std::string &file_path, bool wait_for_keypress_on_shutdown) -> void {
  // Preserve previous behavior by defaulting console colors to enabled.
  Initialize(target_type, platform_services, file_path,
             wait_for_keypress_on_shutdown,
             /*enable_console_colors*/ true);
}

auto Logger::Shutdown() -> void {
  auto &logger = GetStaticInstance();
  std::lock_guard<std::mutex> lock(logger.init_mutex_);
  if (!logger.is_initialized_) {
    std::print(
        "Logger not initialized, cannot shutdown, Consider Initialize() first");
    std::terminate();
  }
  // Signal worker thread to stop
  logger.should_stop_ = true;
  logger.queue_condition_.notify_all();

  // Wait for worker thread to finish
  if (logger.worker_thread_.joinable()) {
    logger.worker_thread_.join();
  }

  // Flush remaining messages
  std::lock_guard<std::mutex> queue_lock(logger.queue_mutex_);
  while (!logger.message_queue_.empty()) {
    const auto &msg = logger.message_queue_.front();
    std::string formatted = logger.FormatMessage(msg.level, msg.file.c_str(),
                                                 msg.line, msg.message);
    for (auto &target : logger.targets_) {
      if (target) {
        target->Write(msg.level, formatted);
      }
    }
    logger.message_queue_.pop();
  }

  // Give each target a chance to perform any shutdown-specific actions.
  if (logger.wait_for_keypress_on_shutdown_) {
    for (auto &target : logger.targets_) {
      if (target) {
        target->OnShutdown(true);
      }
    }
  }

  // Clear targets
  logger.targets_.clear();

  logger.is_initialized_ = false;
}

auto Logger::WorkerThread() -> void {
  while (true) {
    std::unique_lock<std::mutex> queue_lock(queue_mutex_);

    // Wait for messages or shutdown signal
    queue_condition_.wait(queue_lock, [this] {
      return !message_queue_.empty() || should_stop_.load();
    });

    // Process all queued messages
    while (!message_queue_.empty()) {
      const auto msg = message_queue_.front();
      message_queue_.pop();
      queue_lock.unlock();

      // Format the message
      std::string formatted =
          FormatMessage(msg.level, msg.file.c_str(), msg.line, msg.message);

      // Write to all targets
      for (auto &target : targets_) {
        if (target) {
          target->Write(msg.level, formatted);
        }
      }

      queue_lock.lock();
    }

    // Check if we should stop
    if (should_stop_.load()) {
      break;
    }
  }
}

auto Logger::FormatMessage(LogLevel level, const char *file, u32 line,
                           const std::string &message) -> std::string {
  std::string timestamp = GetTimestamp();
  std::string thread_id = GetThreadId();
  const char *level_str = LogLevelToString(level);

  // Format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [TID:thread_id] [file:line]
  // message
  return std::format("[{}] [{}] [TID:{}] [{}:{}] {}\n", timestamp, level_str,
                     thread_id, file, line, message);
}

auto Logger::GetTimestamp() -> std::string {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  // Format time
  std::tm tm_buf{};
#ifdef _WIN32
  localtime_s(&tm_buf, &time_t);
#else
  localtime_r(&time_t, &tm_buf);
#endif

  // Format: YYYY-MM-DD HH:MM:SS.mmm
  return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
                     1900 + tm_buf.tm_year, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                     tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                     static_cast<int>(ms.count()));
}

auto Logger::GetThreadId() -> std::string {
  std::ostringstream oss;
  oss << std::this_thread::get_id();
  return oss.str();
}

Logger::~Logger() {
  // Ensure clean shutdown at process exit if the user did not call
  // Shutdown() explicitly, but never throw from a destructor.
  if (is_initialized_) {
    Shutdown();
  }
}

} // namespace lumina::common::logger
