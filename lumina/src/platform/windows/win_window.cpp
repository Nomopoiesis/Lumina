#include "win_window.hpp"

#include "lumina_util.hpp"

namespace lumina::platform::windows {

static auto WinLuminaWindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam) -> LRESULT {
  LRESULT result = 0;
  switch (msg) {
    case WM_DESTROY: {
      auto &window = Window::Instance();
      window.Close();
      PostQuitMessage(0);
    } break;
    case WM_CLOSE: {
      auto &window = Window::Instance();
      window.Close();
      PostQuitMessage(0);
    } break;
    default: {
      result = DefWindowProcA(hwnd, msg, wparam, lparam);
    } break;
  }
  return result;
}

auto Window::Create(HINSTANCE instance, const char *title, u32 pos_x, u32 pos_y,
                    u32 width, u32 height)
    -> std::expected<std::reference_wrapper<Window>, WindowCreationError> {
  auto &window = GetStaticInstance();

  WNDCLASSEXA window_class = {};
  window_class.cbSize = sizeof(WNDCLASSEXA);
  window_class.style = CS_OWNDC;
  window_class.lpfnWndProc = WinLuminaWindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorA(nullptr, IDC_ARROW); // NOLINT
  window_class.lpszClassName = DEFAULT_WINDOW_CLASS_NAME;

  // Registering window class
  if (RegisterClassExA(&window_class) == 0U) {
    // failed to register window -> return error
    return std::unexpected(
        WindowCreationError{.message = "Failed to register window class"});
  }

  RECT init_window_size;
  init_window_size.left = SafeU32ToI32(pos_x);
  init_window_size.right = SafeU32ToI32(pos_x + width);
  init_window_size.top = SafeU32ToI32(pos_y);
  init_window_size.bottom = SafeU32ToI32(pos_y + height);
  AdjustWindowRectEx(&init_window_size, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0);

  // Create window
  HWND window_handle = CreateWindowExA(
      0, window_class.lpszClassName, title,
      (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS) &
          ~WS_THICKFRAME,
      init_window_size.left, init_window_size.top,
      (init_window_size.right - init_window_size.left),
      (init_window_size.bottom - init_window_size.top), nullptr, nullptr,
      instance, nullptr);

  if (window_handle == nullptr) {
    // failed to create window
    return std::unexpected(
        WindowCreationError{.message = "Failed to create window"});
  }

  window.window_handle = window_handle;
  window.instance = instance;
  window.is_initialized = true;
  window.is_running = true;
  return std::ref(window);
}

auto Window::Destroy() -> void {
  auto &window = GetStaticInstance();
  if (!window.is_initialized) {
    return;
  }
  DestroyWindow(window.window_handle);
  UnregisterClassA(DEFAULT_WINDOW_CLASS_NAME, window.instance);
  window.is_initialized = false;
  window.is_running = false;
}

auto Window::IsRunning() const -> bool { return is_running; }

auto Window::ProcessMessages() -> void {
  MSG msg;
  while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
}

auto Window::GetWindowClientDimensions() const -> core::WindowDimensions {
  RECT window_rect;
  GetClientRect(window_handle, &window_rect);
  return {.width = static_cast<u32>(window_rect.right - window_rect.left),
          .height = static_cast<u32>(window_rect.bottom - window_rect.top)};
}

} // namespace lumina::platform::windows
