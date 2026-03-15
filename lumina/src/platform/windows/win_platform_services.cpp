#include "win_platform_services.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <conio.h>
#include <cstddef>

#include "platform/common/platform_services.hpp"

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

} // namespace

auto InitPlatformServices() -> void {
  lumina::platform::common::PlatformServices::Initialize(
      WinCreateFile, WinOpenFile, WinGetFileSize, WinWriteFile, WinReadFile,
      WinCloseFile, WinDeleteFile, WinCreateConsole, WinWriteConsole,
      WinWaitConsoleKeypress);
}

} // namespace lumina::platform::windows
