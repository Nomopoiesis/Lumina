#include "linux_platform_services.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ucontext.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "common/logger/logger.hpp"
#include "linux_window.hpp"
#include "platform/platform_common/file_handle.hpp"
#include "platform/platform_common/platform_services.hpp"

namespace lumina::platform::llinux {

namespace {

using lumina::platform::common::FileHandle;
using lumina::platform::common::InvalidFileHandle;

// Linux-specific implementation functions

auto LinuxCreateFile(const char *path) -> FileHandle {
  if (path == nullptr) {
    return InvalidFileHandle;
  }

  // Create or open in append mode
  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND,
                0644); // NOLINT(cppcoreguidelines-pro-type-vararg)
  if (fd == -1) {
    return InvalidFileHandle;
  }

  // Seek to end for append mode
  if (lseek(fd, 0, SEEK_END) == -1) {
    close(fd);
    return InvalidFileHandle;
  }

  return static_cast<FileHandle>(fd);
}

auto LinuxCreateDirectory(const char *path) -> bool {
  if (path == nullptr) {
    return false;
  }

  struct stat st{};
  if (stat(path, &st) == 0) {
    return S_ISDIR(st.st_mode);
  }

  return mkdir(path, 0755) == 0;
}

auto LinuxOpenFile(const char *path) -> FileHandle {
  if (path == nullptr) {
    return InvalidFileHandle;
  }

  int fd = open(path, O_RDONLY); // NOLINT(cppcoreguidelines-pro-type-vararg)
  if (fd == -1) {
    return InvalidFileHandle;
  }

  return static_cast<FileHandle>(fd);
}

auto LinuxGetFileSize(FileHandle handle) -> std::size_t {
  if (handle == InvalidFileHandle) {
    return 0;
  }

  struct stat st{};
  if (fstat(static_cast<int>(handle), &st) == -1) {
    return 0;
  }

  return static_cast<std::size_t>(st.st_size);
}

auto LinuxWriteFile(FileHandle handle, const void *data, std::size_t size)
    -> bool {
  if (handle == InvalidFileHandle || data == nullptr || size == 0) {
    return false;
  }

  ssize_t written = write(static_cast<int>(handle), data, size);
  return written == static_cast<ssize_t>(size);
}

auto LinuxReadFile(FileHandle handle, void *data, std::size_t size) -> bool {
  if (handle == InvalidFileHandle || data == nullptr) {
    return false;
  }

  if (size == 0) {
    struct stat st{};
    if (fstat(static_cast<int>(handle), &st) == -1) {
      return false;
    }
    size = static_cast<std::size_t>(st.st_size);
  }

  ssize_t bytes_read = read(static_cast<int>(handle), data, size);
  return bytes_read == static_cast<ssize_t>(size);
}

auto LinuxCloseFile(FileHandle handle) -> void {
  if (handle != InvalidFileHandle) {
    close(static_cast<int>(handle));
  }
}

auto LinuxDeleteFile(const char *path) -> bool {
  if (path == nullptr) {
    return false;
  }

  return unlink(path) == 0;
}

// Statics for the keypress-wait handshake (log FIFO fd + ack FIFO path).
// LinuxWaitConsoleKeypress closes the log FIFO (so cat sees EOF in the
// terminal) then blocks on the ack FIFO until the terminal signals that the
// user has pressed a key.
int g_log_fifo_fd = -1;                       // write end of log FIFO
char g_ack_fifo_path[256] = {};               // path of ack FIFO

auto LinuxCreateConsole() -> FileHandle {
  // --- Step 1: create the log FIFO (main → terminal) ---
  char log_fifo_path[] = "/tmp/lumina_console_XXXXXX";
  {
    int tmp_fd = mkstemp(log_fifo_path); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (tmp_fd == -1) {
      return InvalidFileHandle;
    }
    close(tmp_fd);
    unlink(log_fifo_path);
  }
  if (mkfifo(log_fifo_path, 0600) != 0) {
    return InvalidFileHandle;
  }

  // --- Step 2: create the ack FIFO (terminal → main) ---
  // After the user presses a key in the terminal emulator the shell command
  // writes one byte here, unblocking LinuxWaitConsoleKeypress.
  char ack_fifo_path[] = "/tmp/lumina_ack_XXXXXX";
  {
    int tmp_fd = mkstemp(ack_fifo_path); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (tmp_fd == -1) {
      unlink(log_fifo_path);
      return InvalidFileHandle;
    }
    close(tmp_fd);
    unlink(ack_fifo_path);
  }
  if (mkfifo(ack_fifo_path, 0600) != 0) {
    unlink(log_fifo_path);
    return InvalidFileHandle;
  }
  std::strncpy(g_ack_fifo_path, ack_fifo_path, sizeof(g_ack_fifo_path) - 1);

  // --- Step 3: build the shell command for the terminal emulator ---
  // 1. cat reads the log FIFO until the write end is closed (EOF).
  // 2. dd reads one byte from /dev/tty — this is the user's keypress.
  // 3. printf writes one byte to the ack FIFO, waking LinuxWaitConsoleKeypress.
  char shell_cmd[512];
  std::snprintf( // NOLINT(cppcoreguidelines-pro-type-vararg)
      shell_cmd, sizeof(shell_cmd),
      "cat '%s'; dd bs=1 count=1 if=/dev/tty of=/dev/null 2>/dev/null;"
      " printf x > '%s'",
      log_fifo_path, ack_fifo_path);

  // --- Step 4: fork and exec a terminal emulator ---
  pid_t child = fork();
  if (child == -1) {
    unlink(log_fifo_path);
    unlink(ack_fifo_path);
    g_ack_fifo_path[0] = '\0';
    return InvalidFileHandle;
  }

  if (child == 0) {
    setsid();
    execlp("xterm", "xterm", "-title", "Lumina Console", // NOLINT(cppcoreguidelines-pro-type-vararg)
           "-e", shell_cmd, static_cast<char *>(nullptr));
    execlp("gnome-terminal", "gnome-terminal", // NOLINT(cppcoreguidelines-pro-type-vararg)
           "--", "sh", "-c", shell_cmd, static_cast<char *>(nullptr));
    execlp("ptyxis", "ptyxis", // NOLINT(cppcoreguidelines-pro-type-vararg)
           "--", "sh", "-c", shell_cmd, static_cast<char *>(nullptr));
    execlp("konsole", "konsole", // NOLINT(cppcoreguidelines-pro-type-vararg)
           "-e", "sh", "-c", shell_cmd, static_cast<char *>(nullptr));
    _exit(1);
  }

  // --- Step 5: open the write end of the log FIFO with timeout ---
  int fifo_fd = -1;
  constexpr int kMaxAttempts = 50;
  for (int i = 0; i < kMaxAttempts; ++i) {
    fifo_fd = open(log_fifo_path, O_WRONLY | O_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fifo_fd != -1) {
      break;
    }
    if (errno != ENXIO) {
      break;
    }
    int status = 0;
    if (waitpid(child, &status, WNOHANG) == child) {
      break;
    }
    usleep(100'000);
  }

  // Log FIFO path can be unlinked now; the kernel keeps the pipe alive while
  // both ends are open. The ack FIFO must stay on disk so the terminal shell
  // command can open it by path.
  unlink(log_fifo_path);

  if (fifo_fd == -1) {
    unlink(ack_fifo_path);
    g_ack_fifo_path[0] = '\0';
    return InvalidFileHandle;
  }

  // --- Step 6: restore blocking mode on the log FIFO ---
  int flags = fcntl(fifo_fd, F_GETFL); // NOLINT(cppcoreguidelines-pro-type-vararg)
  if (flags != -1) {
    fcntl(fifo_fd, F_SETFL, flags & ~O_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-vararg)
  }

  g_log_fifo_fd = fifo_fd;
  return static_cast<FileHandle>(fifo_fd);
}

auto LinuxWriteConsole(FileHandle handle, const char *text, std::size_t length)
    -> void {
  if (handle == InvalidFileHandle || text == nullptr || length == 0) {
    return;
  }

  (void)write(static_cast<int>(handle), text, length);
}

auto LinuxWaitConsoleKeypress() -> void {
  // Close the log FIFO write end so cat in the terminal emulator sees EOF and
  // exits, allowing the shell command to advance to the `dd` keypress step.
  if (g_log_fifo_fd != -1) {
    close(g_log_fifo_fd);
    g_log_fifo_fd = -1;
  }

  // Block until the terminal writes one byte to the ack FIFO (which happens
  // after the user presses a key).
  if (g_ack_fifo_path[0] != '\0') {
    int ack_fd = open(g_ack_fifo_path, O_RDONLY); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (ack_fd != -1) {
      char buf[1];
      (void)read(ack_fd, buf, sizeof(buf));
      close(ack_fd);
    }
    unlink(g_ack_fifo_path);
    g_ack_fifo_path[0] = '\0';
  }
}

auto LinuxSetThreadName(const char *name) -> void {
  if (name == nullptr) {
    return;
  }
  // pthread_setname_np truncates to 15 characters + null terminator
  pthread_setname_np(pthread_self(), name);
}

auto LinuxPinThread(std::thread::native_handle_type thread_handle,
                    size_t core_index) -> void {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_index, &cpuset);
  int result =
      pthread_setaffinity_np(thread_handle, sizeof(cpu_set_t), &cpuset);
  if (result != 0) {
    LOG_WARNING("Failed to pin thread to core: {}", result);
  }
}

// --- Linux fiber implementation using POSIX ucontext ---
//
// Each logical fiber is represented by a heap-allocated LinuxFiber that owns
// its ucontext_t and (for non-master fibers) a stack buffer.  The three
// platform primitives map directly:
//
//   LuminaCreateFiber        → allocate LinuxFiber + stack, makecontext
//   LuminaConvertThreadToFiber → allocate LinuxFiber, getcontext (no stack)
//   LuminaSwitchToFiber      → swapcontext(from, to)
//   LuminaDestroyFiber       → delete LinuxFiber (unique_ptr frees stack)

struct LinuxFiber {
  ucontext_t ctx{};
  void (*entry_point)(void *data) = nullptr;
  void *data = nullptr;
  std::unique_ptr<char[]> stack; // null for master (thread-converted) fibers
};

// makecontext only accepts int-sized arguments.  On 64-bit Linux a void*
// must be split into two uint32_t words and reassembled inside the trampoline.
auto FiberTrampoline(uint32_t hi, uint32_t lo) -> void {
  auto ptr = (static_cast<uintptr_t>(hi) << 32) | static_cast<uintptr_t>(lo);
  auto *fiber = reinterpret_cast<LinuxFiber *>(ptr); // NOLINT
  fiber->entry_point(fiber->data);
}

auto LinuxCreateFiber(std::size_t stack_size, void (*entry_point)(void *data),
                      void *data) -> void * {
  auto *fiber = new LinuxFiber{}; // NOLINT
  fiber->entry_point = entry_point;
  fiber->data = data;
  fiber->stack = std::make_unique<char[]>(stack_size);

  getcontext(&fiber->ctx);
  fiber->ctx.uc_stack.ss_sp   = fiber->stack.get();
  fiber->ctx.uc_stack.ss_size = stack_size;
  fiber->ctx.uc_link          = nullptr;

  auto ptr = reinterpret_cast<uintptr_t>(fiber); // NOLINT
  auto hi  = static_cast<uint32_t>(ptr >> 32);
  auto lo  = static_cast<uint32_t>(ptr & 0xFFFFFFFFU);
  makecontext(&fiber->ctx,                                    // NOLINT(cppcoreguidelines-pro-type-vararg)
              reinterpret_cast<void (*)()>(FiberTrampoline), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              2, hi, lo);
  return fiber;
}

auto LinuxConvertThreadToFiber(void *data) -> void * {
  // Capture the calling thread's live context so it can be switched back to
  // via swapcontext.  No stack allocation needed — the thread already has one.
  auto *fiber = new LinuxFiber{}; // NOLINT
  fiber->data = data;
  getcontext(&fiber->ctx);
  return fiber;
}

auto LinuxSwitchToFiber(void *from_fiber, void *to_fiber) -> void {
  auto *from = static_cast<LinuxFiber *>(from_fiber);
  auto *to   = static_cast<LinuxFiber *>(to_fiber);
  swapcontext(&from->ctx, &to->ctx);
}

auto LinuxDestroyFiber(void *fiber) -> void {
  delete static_cast<LinuxFiber *>(fiber); // NOLINT  (stack freed by unique_ptr)
}

auto LinuxSetCursorPosition(f32 x, f32 y) -> void {
  auto &window = Window::Instance();
  xcb_warp_pointer(window.GetXCBConnection(), XCB_NONE,
                   window.GetXCBWindowHandle(), 0, 0, 0, 0,
                   static_cast<i16>(x), static_cast<i16>(y));
  xcb_flush(window.GetXCBConnection());
}

auto LinuxSetCursorTrapped(bool trapped) -> void {
  auto &window = Window::Instance();
  window.SetMouseTrapped(trapped);

  auto *conn = window.GetXCBConnection();
  const xcb_window_t handle = window.GetXCBWindowHandle();

  // Cache the invisible cursor so we only allocate it once per trap session
  static xcb_cursor_t hidden_cursor = XCB_CURSOR_NONE;

  if (trapped) {
    if (hidden_cursor == XCB_CURSOR_NONE) {
      // Create a 1x1 depth-1 pixmap and use it as both src and mask to produce
      // a fully transparent (invisible) cursor
      const xcb_pixmap_t pix = xcb_generate_id(conn);
      xcb_create_pixmap(conn, 1, pix, handle, 1, 1);
      hidden_cursor = xcb_generate_id(conn);
      xcb_create_cursor(conn, hidden_cursor, pix, pix,
                        0, 0, 0, 0, 0, 0, 0, 0);
      xcb_free_pixmap(conn, pix);
    }
    const uint32_t cursor_val = hidden_cursor;
    xcb_change_window_attributes(conn, handle, XCB_CW_CURSOR, &cursor_val);

    // Center cursor so first delta after trapping is zero
    const auto dims = window.GetWindowClientDimensions();
    xcb_warp_pointer(conn, XCB_NONE, handle, 0, 0, 0, 0,
                     static_cast<i16>(dims.width / 2),
                     static_cast<i16>(dims.height / 2));
  } else {
    // Restore the window's default cursor
    const uint32_t cursor_val = XCB_CURSOR_NONE;
    xcb_change_window_attributes(conn, handle, XCB_CW_CURSOR, &cursor_val);
    if (hidden_cursor != XCB_CURSOR_NONE) {
      xcb_free_cursor(conn, hidden_cursor);
      hidden_cursor = XCB_CURSOR_NONE;
    }
  }
  xcb_flush(conn);
}

} // namespace

auto InitPlatformServices() -> void {
  lumina::platform::common::PlatformServices::Initialize(
      LinuxCreateFile, LinuxCreateDirectory, LinuxOpenFile, LinuxGetFileSize,
      LinuxWriteFile, LinuxReadFile, LinuxCloseFile, LinuxDeleteFile,
      LinuxCreateConsole, LinuxWriteConsole, LinuxWaitConsoleKeypress,
      LinuxSetThreadName, LinuxPinThread, LinuxCreateFiber,
      LinuxConvertThreadToFiber, LinuxSwitchToFiber, LinuxDestroyFiber,
      LinuxSetCursorPosition, LinuxSetCursorTrapped);
}

} // namespace lumina::platform::llinux
