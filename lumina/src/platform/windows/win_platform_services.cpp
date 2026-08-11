#include "win_platform_services.hpp"

#include <Windows.h>

#include "common/logger/logger.hpp"
#include "common/lumina_util.hpp"
#include "platform/platform_common/file_handle.hpp"
#include "platform/platform_common/platform_services.hpp"
#include "win_window.hpp"
#include <Shlwapi.h>
#include <conio.h>
#include <cstddef>
#include <wchar.h>

namespace lumina::platform::windows {

namespace {

using lumina::platform::common::FileHandle;
using lumina::platform::common::FileMetadata;
using lumina::platform::common::InvalidFileHandle;

// A Windows HANDLE is a pointer, so it round-trips through the platform-
// agnostic uintptr_t handle. Both NULL and INVALID_HANDLE_VALUE map to the
// InvalidFileHandle sentinel.
auto ToFileHandle(HANDLE handle) -> FileHandle {
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
    return InvalidFileHandle;
  }
  return reinterpret_cast<FileHandle>(handle); // NOLINT
}

// Windows-specific implementation functions

auto WinCreateFile(const char *path) -> FileHandle {
  if (path == nullptr) {
    return InvalidFileHandle;
  }

  // Open file in append mode, create if it doesn't exist
  HANDLE handle =
      CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (handle == INVALID_HANDLE_VALUE) {
    return InvalidFileHandle;
  }

  // Move to end of file for append mode
  SetFilePointer(handle, 0, nullptr, FILE_END);

  return ToFileHandle(handle);
}

auto WinCreateDirectory(const char *path) -> bool {
  if (path == nullptr) {
    return false;
  }
  if (PathFileExistsA(path) == TRUE) {
    return true;
  }
  return CreateDirectoryA(path, nullptr) != FALSE;
}

auto WinOpenFile(const char *path) -> FileHandle {
  if (path == nullptr) {
    return InvalidFileHandle;
  }

  HANDLE handle =
      CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (handle == INVALID_HANDLE_VALUE) {
    return InvalidFileHandle;
  }

  return ToFileHandle(handle);
}

auto WinGetFileSize(FileHandle handle) -> std::size_t {
  if (handle == InvalidFileHandle) {
    return 0;
  }

  HANDLE file_handle = reinterpret_cast<HANDLE>(handle); // NOLINT
  LARGE_INTEGER size;
  if (GetFileSizeEx(file_handle, &size) == FALSE) {
    return 0;
  }
  return static_cast<std::size_t>(size.QuadPart);
}

auto WinWriteFile(FileHandle handle, const void *data, std::size_t size)
    -> bool {
  if (handle == InvalidFileHandle || data == nullptr || size == 0) {
    return false;
  }

  HANDLE file_handle = reinterpret_cast<HANDLE>(handle); // NOLINT
  DWORD bytes_written = 0;

  BOOL result = ::WriteFile(file_handle, data, static_cast<DWORD>(size),
                            &bytes_written, nullptr);

  return result != FALSE && bytes_written == static_cast<DWORD>(size);
}

auto WinReadFile(FileHandle handle, void *data, std::size_t size) -> bool {
  if (handle == InvalidFileHandle || data == nullptr) {
    return false;
  }

  // If file size is 0 read the whole file
  if (size == 0) {
    LARGE_INTEGER win_size;
    if (GetFileSizeEx(reinterpret_cast<HANDLE>(handle), &win_size) == FALSE) {
      return false;
    }
    size = SafeI64ToU64(win_size.QuadPart);
  }

  return ReadFile(reinterpret_cast<HANDLE>(handle), data,
                  static_cast<DWORD>(size), nullptr, nullptr) != FALSE;
}

auto WinCloseFile(FileHandle handle) -> void {
  if (handle != InvalidFileHandle) {
    CloseHandle(reinterpret_cast<HANDLE>(handle));
  }
}

auto WinDeleteFile(const char *path) -> bool {
  if (path == nullptr) {
    return false;
  }

  return DeleteFileA(path) != FALSE;
}

auto WinGetFileMetadata(const char *path, FileMetadata *metadata) -> bool {
  if (path == nullptr || metadata == nullptr) {
    return false;
  }

  // Attribute query rather than CreateFile: no handle to leak, and it succeeds
  // on files another process currently holds open for writing.
  WIN32_FILE_ATTRIBUTE_DATA attributes;
  if (GetFileAttributesExA(path, GetFileExInfoStandard, &attributes) == FALSE) {
    return false;
  }

  ULARGE_INTEGER ticks;
  ticks.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
  ticks.HighPart = attributes.ftLastWriteTime.dwHighDateTime;

  ULARGE_INTEGER size;
  size.LowPart = attributes.nFileSizeLow;
  size.HighPart = attributes.nFileSizeHigh;

  // FILETIME counts 100ns ticks from 1601-01-01. Rebase onto the Unix epoch;
  // anything older than that saturates rather than wrapping the unsigned
  // subtraction.
  constexpr u64 TicksToUnixEpoch = 116444736000000000ULL;
  constexpr u64 NanosecondsPerTick = 100ULL;
  const u64 write_time_ns =
      ticks.QuadPart < TicksToUnixEpoch
          ? 0
          : (ticks.QuadPart - TicksToUnixEpoch) * NanosecondsPerTick;

  *metadata = FileMetadata{.write_time_ns = write_time_ns,
                           .size_bytes = size.QuadPart};
  return true;
}

// Delegates rather than repeating the query and the epoch arithmetic above.
auto WinGetFileWriteTime(const char *path, u64 *write_time_ns) -> bool {
  if (write_time_ns == nullptr) {
    return false;
  }

  FileMetadata metadata;
  if (!WinGetFileMetadata(path, &metadata)) {
    return false;
  }

  *write_time_ns = metadata.write_time_ns;
  return true;
}

auto WinSetConsoleMode(HANDLE handle) -> void {
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD dwMode = 0;
  if (GetConsoleMode(handle, &dwMode) != FALSE) {
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(handle, dwMode) != FALSE) {
      return;
    }
  }
}

auto WinCreateConsole() -> FileHandle {
  // Try to attach to existing console first
  if (AttachConsole(ATTACH_PARENT_PROCESS) != FALSE) {
    WinSetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE));
    return ToFileHandle(GetStdHandle(STD_OUTPUT_HANDLE));
  }

  // If no parent console, allocate a new one
  if (AllocConsole() != FALSE) {
    WinSetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE));
    return ToFileHandle(GetStdHandle(STD_OUTPUT_HANDLE));
  }

  // Fallback: try to get existing stdout handle
  WinSetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE));
  return ToFileHandle(GetStdHandle(STD_OUTPUT_HANDLE));
}

auto WinWriteConsole(FileHandle handle, const char *text, std::size_t length)
    -> void {
  if (handle == InvalidFileHandle || text == nullptr || length == 0) {
    return;
  }

  HANDLE console_handle = reinterpret_cast<HANDLE>(handle); // NOLINT
  DWORD bytes_written = 0;

  // Try WriteConsoleA first (works for console handles)
  if (WriteConsoleA(console_handle, text, static_cast<DWORD>(length),
                    &bytes_written, nullptr) != FALSE) {
    return;
  }

  // Fallback to WriteFile for redirected output
  WriteFile(console_handle, text, static_cast<DWORD>(length), &bytes_written,
            nullptr);
}

auto WinWaitConsoleKeypress() -> void {
  // Ensure there is a console attached/allocated first. WinCreateConsole is
  // responsible for that, so by the time this is called we can safely assume
  // a console exists when logging is configured to use one.
  (void)_getch();
}

auto WinAnsiToWide(const char *ansiString) -> std::wstring {
  // Determine the required buffer size for the wide string
  // CP_ACP specifies the current Windows ANSI code page
  int bufferSize = MultiByteToWideChar(CP_ACP, 0, ansiString, -1, nullptr, 0);

  if (bufferSize == 0) {
    return {};
  }

  std::wstring wideString(SafeI32ToU64(bufferSize), L'\0');
  MultiByteToWideChar(CP_ACP, 0, ansiString, -1, wideString.data(), bufferSize);
  return wideString;
}

auto WinSetThreadName(const char *name) -> void {
  if (name == nullptr) {
    return;
  }
  auto wide_name = WinAnsiToWide(name);
  if (wide_name.empty()) {
    return;
  }
  SetThreadDescription(GetCurrentThread(), wide_name.c_str());
}

auto WinPinThread(std::thread::native_handle_type thread_handle,
                  size_t core_index) -> void {
  auto result = SetThreadAffinityMask(thread_handle, 1 << core_index);
  if (result == 0) {
    LOG_WARNING("Failed to pin thread to core: {}", GetLastError());
  }
}

auto WinCreateFiber(std::size_t stack_size, void (*entry_point)(void *data),
                    void *data) -> void * {
  return CreateFiberEx(stack_size, 0, FIBER_FLAG_FLOAT_SWITCH, entry_point,
                       data);
}

auto WinConvertThreadToFiber(void *data) -> void * {
  return ConvertThreadToFiberEx(data, FIBER_FLAG_FLOAT_SWITCH);
}

auto WinSwitchToFiber(void *from_fiber [[maybe_unused]], void *to_fiber)
    -> void {
  SwitchToFiber(to_fiber);
}

auto WinDestroyFiber(void *fiber) -> void {
  DeleteFiber(fiber);
}

// Fiber-local storage slot holding the running fiber's identity.
//
// Deliberately FLS and not thread_local: the value has to follow the fiber
// across threads, and FlsGetValue is an opaque call the optimizer cannot hoist
// across a fiber switch the way it hoists a TLS block address.
DWORD g_fiber_self_slot = FLS_OUT_OF_INDEXES;

auto WinSetFiberSelf(void *self) -> void {
  if (g_fiber_self_slot == FLS_OUT_OF_INDEXES) {
    return;
  }
  FlsSetValue(g_fiber_self_slot, self);
}

auto WinGetFiberSelf() -> void * {
  if (g_fiber_self_slot == FLS_OUT_OF_INDEXES) {
    return nullptr;
  }
  return FlsGetValue(g_fiber_self_slot);
}

auto WinSetCursorPosition(f32 x, f32 y) -> void {
  POINT point = {static_cast<int>(x), static_cast<int>(y)};
  auto *window = Window::Instance().GetWindowHandle();
  ClientToScreen(window, &point);
  SetCursorPos(point.x, point.y);
}

auto WinSetCursorTrapped(bool trapped) -> void {
  Window::Instance().SetMouseTrapped(trapped);
  ShowCursor(trapped ? FALSE : TRUE);
  if (trapped) {
    RECT window_rect;
    GetClientRect(Window::Instance().GetWindowHandle(), &window_rect);
    POINT center = {
        (window_rect.left + window_rect.right) / 2,
        (window_rect.top + window_rect.bottom) / 2,
    };
    ClientToScreen(Window::Instance().GetWindowHandle(), &center);
    SetCursorPos(center.x, center.y);
  }
}

} // namespace

auto InitPlatformServices() -> void {
  g_fiber_self_slot = FlsAlloc(nullptr);

  lumina::platform::common::PlatformServices::Initialize(
      WinCreateFile, WinCreateDirectory, WinOpenFile, WinGetFileSize,
      WinWriteFile, WinReadFile, WinCloseFile, WinDeleteFile,
      WinGetFileWriteTime, WinGetFileMetadata, WinCreateConsole, WinWriteConsole,
      WinWaitConsoleKeypress, WinSetThreadName, WinPinThread,
      WinCreateFiber, WinConvertThreadToFiber, WinSwitchToFiber,
      WinDestroyFiber, WinSetFiberSelf, WinGetFiberSelf, WinSetCursorPosition,
      WinSetCursorTrapped);
}

} // namespace lumina::platform::windows
