#include "win_platform_services.hpp"

#include <Windows.h>

#include "common/logger/logger.hpp"
#include "platform/common/platform_services.hpp"
#include "win_window.hpp"
#include <conio.h>
#include <cstddef>
#include <wchar.h>

namespace lumina::platform::windows {

namespace {

// Windows-specific implementation functions

auto WinCreateFile(const char *path) -> void * {
  if (path == nullptr) {
    return nullptr;
  }

  // Open file in append mode, create if it doesn't exist
  HANDLE handle =
      CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (handle == INVALID_HANDLE_VALUE) {
    return nullptr;
  }

  // Move to end of file for append mode
  SetFilePointer(handle, 0, nullptr, FILE_END);

  return static_cast<void *>(handle);
}

auto WinOpenFile(const char *path) -> void * {
  if (path == nullptr) {
    return nullptr;
  }

  HANDLE handle =
      CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (handle == INVALID_HANDLE_VALUE) {
    return nullptr;
  }

  return static_cast<void *>(handle);
}

auto WinGetFileSize(void *handle) -> std::size_t {
  if (handle == nullptr) {
    return 0;
  }

  HANDLE file_handle = static_cast<HANDLE>(handle); // NOLINT
  LARGE_INTEGER size;
  if (GetFileSizeEx(file_handle, &size) == FALSE) {
    return 0;
  }
  return static_cast<std::size_t>(size.QuadPart);
}

auto WinWriteFile(void *handle, const void *data, std::size_t size) -> bool {
  if (handle == nullptr || data == nullptr || size == 0) {
    return false;
  }

  HANDLE file_handle = static_cast<HANDLE>(handle); // NOLINT
  DWORD bytes_written = 0;

  BOOL result = ::WriteFile(file_handle, data, static_cast<DWORD>(size),
                            &bytes_written, nullptr);

  return result != FALSE && bytes_written == static_cast<DWORD>(size);
}

auto WinReadFile(void *handle, void *data, std::size_t size) -> bool {
  if (handle == nullptr || data == nullptr) {
    return false;
  }

  // If file size is 0 read the whole file
  if (size == 0) {
    size = GetFileSizeEx(static_cast<HANDLE>(handle), nullptr);
    if (size == INVALID_FILE_SIZE) {
      return false;
    }
  }

  return ReadFile(static_cast<HANDLE>(handle), data, static_cast<DWORD>(size),
                  nullptr, nullptr) != FALSE;
}

auto WinCloseFile(void *handle) -> void {
  if (handle != nullptr) {
    CloseHandle(static_cast<HANDLE>(handle));
  }
}

auto WinDeleteFile(const char *path) -> bool {
  if (path == nullptr) {
    return false;
  }

  return DeleteFileA(path) != FALSE;
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

auto WinCreateConsole() -> void * {
  // Try to attach to existing console first
  if (AttachConsole(ATTACH_PARENT_PROCESS) != FALSE) {
    WinSetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE));
    return GetStdHandle(STD_OUTPUT_HANDLE);
  }

  // If no parent console, allocate a new one
  if (AllocConsole() != FALSE) {
    WinSetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE));
    return GetStdHandle(STD_OUTPUT_HANDLE);
  }

  // Fallback: try to get existing stdout handle
  WinSetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE));
  return GetStdHandle(STD_OUTPUT_HANDLE);
}

auto WinWriteConsole(void *handle, const char *text, std::size_t length)
    -> void {
  if (handle == nullptr || text == nullptr || length == 0) {
    return;
  }

  HANDLE console_handle = static_cast<HANDLE>(handle); // NOLINT
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

  std::wstring wideString(bufferSize, L'\0');
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
  lumina::platform::common::PlatformServices::Initialize(
      WinCreateFile, WinOpenFile, WinGetFileSize, WinWriteFile, WinReadFile,
      WinCloseFile, WinDeleteFile, WinCreateConsole, WinWriteConsole,
      WinWaitConsoleKeypress, WinSetThreadName, WinPinThread, WinCreateFiber,
      WinConvertThreadToFiber, WinSwitchToFiber, WinSetCursorPosition,
      WinSetCursorTrapped);
}

} // namespace lumina::platform::windows
