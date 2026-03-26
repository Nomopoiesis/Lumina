#include "linux_platform_services.hpp"

#include <cerrno>
#include <cstdio>
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

auto LinuxCreateConsole() -> FileHandle {
  // --- Step 1: create a named FIFO in /tmp ---
  // A FIFO (named pipe) is a special file that acts as a unidirectional byte
  // channel: one process writes to it, another reads. We use it so the
  // terminal emulator we spawn can consume our log data without any shared
  // memory or socket setup.
  //
  // mkstemp gives us a unique, collision-free path, which we immediately
  // delete and recreate as a FIFO with the same name.
  char fifo_path[] = "/tmp/lumina_console_XXXXXX";
  {
    int tmp_fd = mkstemp(fifo_path); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (tmp_fd == -1) {
      return InvalidFileHandle;
    }
    close(tmp_fd);
    unlink(fifo_path); // remove the regular file so mkfifo can create the FIFO
  }
  if (mkfifo(fifo_path, 0600) != 0) {
    return InvalidFileHandle;
  }

  // --- Step 2: build the shell command for the terminal emulator ---
  // The terminal will execute `cat '<fifo_path>'`.
  // cat opens the FIFO for reading, then copies every byte it receives to its
  // stdout, which is the terminal emulator's display. Because cat's stdout is
  // a PTY (a pseudo-terminal created by the emulator), it is line-buffered —
  // each '\n' in a log line triggers an immediate flush to the screen.
  char shell_cmd[sizeof(fifo_path) + 8];
  std::snprintf(shell_cmd, sizeof(shell_cmd), "cat '%s'", fifo_path); // NOLINT(cppcoreguidelines-pro-type-vararg)

  // --- Step 3: fork and exec a terminal emulator ---
  // The child detaches into a new session (setsid) so that keyboard signals
  // such as Ctrl-C sent to the game's controlling terminal do not propagate
  // to the console window.
  // We try three common emulators in order; the first exec that succeeds wins.
  pid_t child = fork();
  if (child == -1) {
    unlink(fifo_path);
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
    _exit(1); // none of the emulators were found
  }

  // --- Step 4: open the write end of the FIFO with timeout ---
  // Normally opening a FIFO for writing blocks until a reader opens it.
  // We use O_NONBLOCK + retry instead so we can detect the case where no
  // terminal emulator is installed (child exits, FIFO never gets a reader).
  // The loop retries for up to 5 seconds (50 x 100 ms).
  int fifo_fd = -1;
  constexpr int kMaxAttempts = 50;
  for (int i = 0; i < kMaxAttempts; ++i) {
    fifo_fd = open(fifo_path, O_WRONLY | O_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fifo_fd != -1) {
      break; // reader (cat) is ready
    }
    if (errno != ENXIO) {
      break; // unexpected error — stop retrying
    }
    int status = 0;
    if (waitpid(child, &status, WNOHANG) == child) {
      break; // terminal emulator exited without opening the FIFO
    }
    usleep(100'000); // wait 100 ms before next attempt
  }

  // The FIFO path can be removed from the filesystem now that both ends hold
  // open file descriptors. The kernel keeps the underlying pipe alive until
  // the last fd referencing it is closed.
  unlink(fifo_path);

  if (fifo_fd == -1) {
    return InvalidFileHandle;
  }

  // --- Step 5: restore blocking mode ---
  // We opened with O_NONBLOCK only to avoid hanging during startup.
  // Subsequent writes from LinuxWriteConsole should block naturally if the
  // FIFO kernel buffer is full, providing back-pressure rather than silently
  // dropping log data.
  int flags = fcntl(fifo_fd, F_GETFL); // NOLINT(cppcoreguidelines-pro-type-vararg)
  if (flags != -1) {
    fcntl(fifo_fd, F_SETFL, flags & ~O_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-vararg)
  }

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
  struct termios oldt{};
  struct termios newt{};
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  (void)getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
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

// Fiber support on Linux requires ucontext or a third-party library.
// These are stub implementations — fiber functionality is not yet supported.

auto LinuxCreateFiber(std::size_t /*stack_size*/,
                      void (* /*entry_point*/)(void *data), void * /*data*/)
    -> void * {
  LOG_WARNING("LinuxCreateFiber: fibers not yet implemented on Linux");
  return nullptr;
}

auto LinuxConvertThreadToFiber(void * /*data*/) -> void * {
  LOG_WARNING("LinuxConvertThreadToFiber: fibers not yet implemented on Linux");
  return nullptr;
}

auto LinuxSwitchToFiber(void * /*from_fiber*/, void * /*to_fiber*/) -> void {
  LOG_WARNING("LinuxSwitchToFiber: fibers not yet implemented on Linux");
}

auto LinuxSetCursorPosition(f32 /*x*/, f32 /*y*/) -> void {
  // TODO: implement via XCB warp pointer
  LOG_WARNING("LinuxSetCursorPosition: not yet implemented on Linux");
}

auto LinuxSetCursorTrapped(bool trapped) -> void {
  Window::Instance().SetMouseTrapped(trapped);
}

} // namespace

auto InitPlatformServices() -> void {
  lumina::platform::common::PlatformServices::Initialize(
      LinuxCreateFile, LinuxCreateDirectory, LinuxOpenFile, LinuxGetFileSize,
      LinuxWriteFile, LinuxReadFile, LinuxCloseFile, LinuxDeleteFile,
      LinuxCreateConsole, LinuxWriteConsole, LinuxWaitConsoleKeypress,
      LinuxSetThreadName, LinuxPinThread, LinuxCreateFiber,
      LinuxConvertThreadToFiber, LinuxSwitchToFiber, LinuxSetCursorPosition,
      LinuxSetCursorTrapped);
}

} // namespace lumina::platform::llinux
