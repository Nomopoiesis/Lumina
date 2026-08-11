#pragma once

#include "common/lumina_terminate.hpp"
#include "common/lumina_types.hpp"
#include "platform/platform_common/file_handle.hpp"
#include <cstddef>
#include <print>
#include <thread>

namespace lumina::platform::common {

// What a single filesystem query about an existing file yields. Both platforms
// return these two facts from one call — GetFileAttributesEx on Windows, stat
// on Linux — so a caller that needs both should ask for them together rather
// than issuing two queries that can disagree with each other.
struct FileMetadata {
  // Last modification time, in nanoseconds since the Unix epoch. See
  // LuminaGetFileWriteTime for why implementations must rebase onto that unit.
  u64 write_time_ns = 0;
  u64 size_bytes = 0;
};

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
      FileHandle (*create_file)(const char *path),
      bool (*create_directory)(const char *path),
      FileHandle (*open_file)(const char *path),
      std::size_t (*get_file_size)(FileHandle handle),
      bool (*write_file)(FileHandle handle, const void *data, std::size_t size),
      bool (*read_file)(FileHandle handle, void *data, std::size_t size),
      void (*close_file)(FileHandle handle),
      bool (*delete_file)(const char *path),
      bool (*get_file_write_time)(const char *path, u64 *write_time_ns),
      bool (*get_file_metadata)(const char *path, FileMetadata *metadata),
      FileHandle (*create_console)(),
      void (*write_console)(FileHandle handle, const char *text,
                            std::size_t length),
      void (*wait_console_keypress)(),
      void (*set_thread_name)(const char *name),
      void (*pin_thread)(std::thread::native_handle_type thread_handle,
                         size_t core_index),
      void *(*create_fiber)(std::size_t stack_size,
                            void (*entry_point)(void *data), void *data),
      void *(*convert_thread_to_fiber)(void *data),
      void (*switch_to_fiber)(void *from_fiber, void *to_fiber),
      void (*destroy_fiber)(void *fiber), void (*set_fiber_self)(void *self),
      void *(*get_fiber_self)(),
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
    instance.LuminaGetFileWriteTime = get_file_write_time;
    instance.LuminaGetFileMetadata = get_file_metadata;
    instance.LuminaCreateConsole = create_console;
    instance.LuminaWriteConsole = write_console;
    instance.LuminaWaitConsoleKeypress = wait_console_keypress;
    instance.LuminaSetThreadName = set_thread_name;
    instance.LuminaPinThread = pin_thread;
    instance.LuminaCreateFiber = create_fiber;
    instance.LuminaConvertThreadToFiber = convert_thread_to_fiber;
    instance.LuminaSwitchToFiber = switch_to_fiber;
    instance.LuminaDestroyFiber = destroy_fiber;
    instance.LuminaSetFiberSelf = set_fiber_self;
    instance.LuminaGetFiberSelf = get_fiber_self;
    instance.LuminaSetCursorPosition = set_cursor_position;
    instance.LuminaSetCursorTrapped = set_cursor_trapped;
    instance.is_initialized_ = true;
  }

  // File operations
  // Creates/opens a file at the given path and returns a platform-specific
  // handle. Returns InvalidFileHandle on failure.
  FileHandle (*LuminaCreateFile)(const char *path) = nullptr;

  // Creates a directory at the given path
  // Returns true on success, false on failure
  bool (*LuminaCreateDirectory)(const char *path) = nullptr;

  // Opens a file at the given path and returns a platform-specific handle
  // Returns InvalidFileHandle on failure.
  FileHandle (*LuminaOpenFile)(const char *path) = nullptr;

  // Gets the size of a file handle
  std::size_t (*LuminaGetFileSize)(FileHandle handle) = nullptr;

  // Writes data to a file handle
  // Returns true on success, false on failure
  bool (*LuminaWriteFile)(FileHandle handle, const void *data,
                          std::size_t size) = nullptr;

  // Reads data from a file handle
  // Returns true on success, false on failure
  bool (*LuminaReadFile)(FileHandle handle, void *data,
                         std::size_t size) = nullptr;

  // Closes a file handle
  void (*LuminaCloseFile)(FileHandle handle) = nullptr;

  // Deletes a file at the given path
  // Returns true on success, false on failure
  // Optional: used for log rotation
  bool (*LuminaDeleteFile)(const char *path) = nullptr;

  // Writes the file's last-modification time, in nanoseconds since the Unix
  // epoch, to write_time_ns. Platform clocks count from different epochs at
  // different resolutions, so implementations must rebase onto that unit for
  // timestamps to be comparable across platforms.
  // Returns false if the file does not exist or cannot be queried, leaving
  // write_time_ns untouched.
  bool (*LuminaGetFileWriteTime)(const char *path,
                                 u64 *write_time_ns) = nullptr;

  // Writes everything one filesystem query can say about an existing file to
  // metadata. For a caller that wants more than the timestamp this is a single
  // query rather than several, so the fields it returns describe the same
  // observation of the file rather than separate ones taken moments apart.
  // Returns false if the file does not exist or cannot be queried, leaving
  // metadata untouched.
  bool (*LuminaGetFileMetadata)(const char *path,
                                FileMetadata *metadata) = nullptr;

  // Console operations
  // Creates/attaches to a console and returns a platform-specific handle
  // Returns InvalidFileHandle on failure.
  FileHandle (*LuminaCreateConsole)() = nullptr;

  // Writes text to a console handle
  void (*LuminaWriteConsole)(FileHandle handle, const char *text,
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

  // Destroys a fiber created by LuminaCreateFiber or LuminaConvertThreadToFiber
  // and frees any associated resources (stack, context struct, etc.)
  void (*LuminaDestroyFiber)(void *fiber) = nullptr;

  // Fiber-local storage for a single pointer identifying the running fiber.
  //
  // This exists because thread_local is unsafe on a fiber stack: a fiber can be
  // resumed on a different thread than it last ran on, while compilers cache
  // the thread's TLS block address across calls. These must resolve against the
  // *currently executing fiber*, not the thread, and must not be cacheable
  // across a fiber switch. Returns nullptr on a thread that is not running a
  // job fiber.
  void (*LuminaSetFiberSelf)(void *self) = nullptr;
  void *(*LuminaGetFiberSelf)() = nullptr;

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
