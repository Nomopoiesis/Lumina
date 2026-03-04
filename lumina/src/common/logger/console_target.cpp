#include "console_target.hpp"

namespace lumina::common::logger {

namespace {

// Returns the ANSI SGR sequence for the given log level, or an empty string
// when the default console color should be used.
constexpr auto GetAnsiCodeForLevel(LogLevel level) -> const char * {
  using lumina::common::logger::LogLevel;

  switch (level) {
    case LogLevel::TRACE:
    case LogLevel::DEBUG:
      return "\x1b[90m"; // bright black / dim gray
    case LogLevel::INFO:
      return ""; // default color
    case LogLevel::WARNING:
      return "\x1b[33m"; // yellow
    case LogLevel::ERROR:
      return "\x1b[31m"; // red
    case LogLevel::CRITICAL:
      return "\x1b[91;1m"; // bright red, bold
  }

  return "";
}

} // namespace

ConsoleTarget::ConsoleTarget(
    lumina::platform::common::PlatformServices &platform_services,
    const bool enable_colors)
    : platform_services_(platform_services), console_handle_(nullptr),
      enable_colors_(enable_colors) {
  // Create/attach to console using platform services
  if (platform_services_.LuminaCreateConsole != nullptr) {
    console_handle_ = platform_services_.LuminaCreateConsole();
  }
}

auto ConsoleTarget::Write(LogLevel level, const std::string &message) -> void {
  if (console_handle_ == nullptr ||
      platform_services_.LuminaWriteConsole == nullptr) {
    return;
  }

  // Thread-safe write using mutex
  std::lock_guard<std::mutex> lock(write_mutex_);

  if (!enable_colors_) {
    platform_services_.LuminaWriteConsole(console_handle_, message.c_str(),
                                          message.length());
    return;
  }

  const char *const ansi_prefix = GetAnsiCodeForLevel(level);

  if (ansi_prefix == nullptr || ansi_prefix[0] == '\0') {
    platform_services_.LuminaWriteConsole(console_handle_, message.c_str(),
                                          message.length());
    return;
  }

  constexpr const char *kAnsiReset = "\x1b[0m";

  std::string colored_message;
  colored_message.reserve(std::char_traits<char>::length(ansi_prefix) +
                          message.length() +
                          std::char_traits<char>::length(kAnsiReset));

  colored_message.append(ansi_prefix);
  colored_message.append(message);
  colored_message.append(kAnsiReset);

  platform_services_.LuminaWriteConsole(
      console_handle_, colored_message.c_str(), colored_message.length());
}

auto ConsoleTarget::OnShutdown(bool wait_for_keypress) -> void {
  if (!wait_for_keypress) {
    return;
  }

  if (console_handle_ == nullptr ||
      platform_services_.LuminaWriteConsole == nullptr) {
    return;
  }

  constexpr const char *kShutdownMessage =
      "Press any key to close Lumina . . .";

  {
    // Reuse the same mutex to avoid interleaving with normal writes.
    std::lock_guard<std::mutex> lock(write_mutex_);
    platform_services_.LuminaWriteConsole(
        console_handle_, kShutdownMessage,
        std::char_traits<char>::length(kShutdownMessage));
  }

  if (platform_services_.LuminaWaitConsoleKeypress != nullptr) {
    platform_services_.LuminaWaitConsoleKeypress();
  }
}

} // namespace lumina::common::logger
