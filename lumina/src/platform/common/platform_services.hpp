#pragma once

#include "common/lumina_terminate.hpp"
#include "common/lumina_types.hpp"
#include <cstddef>
#include <print>
#include <thread>

namespace lumina::platform::common {

// Platform-agnostic interface for platform-specific I/O operations
// Platform-specific implementations (e.g., Windows) will provide concrete
// function implementations for these operations
class PlatformServices {
public:
  // Delete copy and move constructors/operators
  PlatformServices(const PlatformServices &) = delete;
  auto operator=(const PlatformServices &) -> PlatformServices & = delete;
  PlatformServices(PlatformServices &&) = delete;
  auto operator=(PlatformServices &&) -> PlatformServices & = delete;

  // Get the singleton instance
  // Throws if not initialized
  static auto Instance() -> PlatformServices & {
    auto &instance = GetStaticInstance();
    if (!instance.is_initialized_) {
      std::print("PlatformServices not initialized, call Initialize() first");
      LUMINA_TERMINATE();
    }
    return instance;
  }

  // Initialize the singleton with platform-specific function pointers
  // Must be called before using the singleton
  static auto Initialize(
      void *(*create_file)(const char *path),
      bool (*create_directory)(const char *path),
      void *(*open_file)(const char *path),
      std::size_t (*get_file_size)(void *handle),
      bool (*write_file)(void *handle, const void *data, std::size_t size),
      bool (*read_file)(void *handle, void *data, std::size_t size),
      void (*close_file)(void *handle), bool (*delete_file)(const char *path),
      void *(*create_console)(),
      void (*write_console)(void *handle, const char *text, std::size_t length),
      void (*wait_console_keypress)(),
      void (*set_thread_name)(const char *name),
      void (*pin_thread)(std::thread::native_handle_type thread_handle,
                         size_t core_index),
      void *(*create_fiber)(std::size_t stack_size,
                            void (*entry_point)(void *data), void *data),
      void *(*convert_thread_to_fiber)(void *data),
      void (*switch_to_fiber)(void *from_fiber, void *to_fiber),
      void (*set_cursor_position)(f32 x, f32 y),
      void (*set_cursor_trapped)(bool trapped)) -> void {
    auto &instance = GetStaticInstance();
    instance.LuminaCreateFile = create_file;
    instance.LuminaCreateDirectory = create_directory;
    instance.LuminaOpenFile = open_file;
    instance.LuminaGetFileSize = get_file_size;
    instance.LuminaWriteFile = write_file;
    instance.LuminaReadFile = read_file;
    instance.LuminaCloseFile = close_file;
    instance.LuminaDeleteFile = delete_file;
    instance.LuminaCreateConsole = create_console;
    instance.LuminaWriteConsole = write_console;
    instance.LuminaWaitConsoleKeypress = wait_console_keypress;
    instance.LuminaSetThreadName = set_thread_name;
    instance.LuminaPinThread = pin_thread;
    instance.LuminaCreateFiber = create_fiber;
    instance.LuminaConvertThreadToFiber = convert_thread_to_fiber;
    instance.LuminaSwitchToFiber = switch_to_fiber;
    instance.LuminaSetCursorPosition = set_cursor_position;
    instance.LuminaSetCursorTrapped = set_cursor_trapped;
    instance.is_initialized_ = true;
  }

  // File operations
  // Creates/opens a file at the given path and returns a platform-specific
  // handle Returns nullptr on failure
  void *(*LuminaCreateFile)(const char *path) = nullptr;

  // Creates a directory at the given path
  // Returns true on success (also when the directory already exists), false on
  // failure
  bool (*LuminaCreateDirectory)(const char *path) = nullptr;

  // Opens a file at the given path and returns a platform-specific handle
  // Returns nullptr on failure
  void *(*LuminaOpenFile)(const char *path) = nullptr;

  // Gets the size of a file handle
  std::size_t (*LuminaGetFileSize)(void *handle) = nullptr;

  // Writes data to a file handle
  // Returns true on success, false on failure
  bool (*LuminaWriteFile)(void *handle, const void *data,
                          std::size_t size) = nullptr;

  // Reads data from a file handle
  // Returns true on success, false on failure
  bool (*LuminaReadFile)(void *handle, void *data, std::size_t size) = nullptr;

  // Closes a file handle
  void (*LuminaCloseFile)(void *handle) = nullptr;

  // Deletes a file at the given path
  // Returns true on success, false on failure
  // Optional: used for log rotation
  bool (*LuminaDeleteFile)(const char *path) = nullptr;

  // Console operations
  // Creates/attaches to a console and returns a platform-specific handle
  // Returns nullptr on failure
  void *(*LuminaCreateConsole)() = nullptr;

  // Writes text to a console handle
  void (*LuminaWriteConsole)(void *handle, const char *text,
                             std::size_t length) = nullptr;

  // Waits for a key press on the console, typically used at shutdown to keep
  // the console window open long enough to inspect log output.
  void (*LuminaWaitConsoleKeypress)() = nullptr;

  // Threading operations

  // Specify name of the thread - for debugging purposes
  void (*LuminaSetThreadName)(const char *name) = nullptr;

  // Pin a thread to a specific core
  void (*LuminaPinThread)(std::thread::native_handle_type thread_handle,
                          size_t core_index) = nullptr;

  // Crete a new fiber
  // Returns a fiber handle
  void *(*LuminaCreateFiber)(std::size_t stack_size,
                             void (*entry_point)(void *data),
                             void *data) = nullptr;

  // Converts a thread to a fiber
  // Returns a fiber handle
  void *(*LuminaConvertThreadToFiber)(void *data) = nullptr;

  // Switches to a fiber
  // from_fiber: the fiber to switch from
  // to_fiber: the fiber to switch to
  void (*LuminaSwitchToFiber)(void *from_fiber, void *to_fiber) = nullptr;

  // Cursor operations
  // Sets the cursor position
  void (*LuminaSetCursorPosition)(f32 x, f32 y) = nullptr;
  // Sets the cursor trapped
  void (*LuminaSetCursorTrapped)(bool trapped) = nullptr;

private:
  PlatformServices() = default;
  ~PlatformServices() = default;

  static auto GetStaticInstance() -> PlatformServices & {
    static PlatformServices instance;
    return instance;
  }

  bool is_initialized_{false};
};

} // namespace lumina::platform::common
