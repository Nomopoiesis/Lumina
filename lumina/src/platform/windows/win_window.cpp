#include "win_window.hpp"

#include "lumina_util.hpp"
#include "win_input_map.hpp"

#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM, GET_WHEEL_DELTA_WPARAM

namespace lumina::platform::windows {

static auto WinLuminaWindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                LPARAM lparam) -> LRESULT {
  LRESULT result = 0;
  if (!core::LuminaEngine::IsInitialized()) {
    return DefWindowProcA(hwnd, msg, wparam, lparam);
  }

  auto &engine = core::LuminaEngine::Instance();
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
    case WM_SIZE: {
      engine.WindowResized();
    } break;

    // ---- Keyboard ----
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      // wparam is the virtual-key code; mask to u8 range for table lookup
      if (wparam < 256) {
        const auto key_code = VK_TO_KEY_CODE[wparam];
        if (key_code != UNMAPPED_KEY) {
          // Bit 30 of lparam: 1 = key was already down before this message
          const bool was_down = (lparam & (1 << 30)) != 0;
          const auto state =
              was_down ? core::KeyState::Held : core::KeyState::Pressed;
          engine.GetInputState().SetKeyState(key_code, state);
        }
      }
    } break;
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      if (wparam < 256) {
        const auto key_code = VK_TO_KEY_CODE[wparam];
        if (key_code != UNMAPPED_KEY) {
          engine.GetInputState().SetKeyState(key_code,
                                             core::KeyState::Released);
        }
      }
    } break;

    // ---- Mouse buttons ----
    case WM_LBUTTONDOWN:
      engine.GetInputState().SetMouseButtonState(core::MouseButton::Left,
                                                 core::KeyState::Pressed);
      break;
    case WM_LBUTTONUP:
      engine.GetInputState().SetMouseButtonState(core::MouseButton::Left,
                                                 core::KeyState::Released);
      break;
    case WM_RBUTTONDOWN:
      engine.GetInputState().SetMouseButtonState(core::MouseButton::Right,
                                                 core::KeyState::Pressed);
      break;
    case WM_RBUTTONUP:
      engine.GetInputState().SetMouseButtonState(core::MouseButton::Right,
                                                 core::KeyState::Released);
      break;
    case WM_MBUTTONDOWN:
      engine.GetInputState().SetMouseButtonState(core::MouseButton::Middle,
                                                 core::KeyState::Pressed);
      break;
    case WM_MBUTTONUP:
      engine.GetInputState().SetMouseButtonState(core::MouseButton::Middle,
                                                 core::KeyState::Released);
      break;

    // ---- Mouse movement ----
    // case WM_MOUSEMOVE - processed in ProcessMouseMovement()

    // ---- Scroll wheel ----
    case WM_MOUSEWHEEL: {
      const i32 delta = GET_WHEEL_DELTA_WPARAM(wparam);
      engine.GetInputState().SetMouseScroll(0, delta);
    } break;
    case WM_MOUSEHWHEEL: {
      const i32 delta = GET_WHEEL_DELTA_WPARAM(wparam);
      engine.GetInputState().SetMouseScroll(delta, 0);
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
      (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
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

auto Window::ProcessMouseMovement() -> void {
  auto &input = core::LuminaEngine::Instance().GetInputState();
  if (mouse_trapped) {
    // For captured mouse, use a fixed center point approach
    RECT client_rect;
    GetClientRect(window_handle, &client_rect);
    POINT center = {
        (client_rect.left + client_rect.right) / 2,
        (client_rect.top + client_rect.bottom) / 2,
    };

    // Get current cursor position
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    ScreenToClient(window_handle, &cursor_pos);

    // Calculate delta from center (this is the key change!)
    auto delta_x = cursor_pos.x - center.x;
    auto delta_y = cursor_pos.y - center.y;

    input.SetMouseDelta(delta_x, delta_y);

    // Set position to center for display purposes
    input.SetMousePosition(center.x, center.y);

    // Recenter cursor for next frame
    ClientToScreen(window_handle, &center);
    SetCursorPos(center.x, center.y);
  } else {
    // For non-captured mouse, use normal position tracking
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    ScreenToClient(window_handle, &cursor_pos);

    // Calculate delta movement
    auto delta_x = cursor_pos.x - previous_mouse_position.x;
    auto delta_y = cursor_pos.y - previous_mouse_position.y;
    input.SetMouseDelta(delta_x, delta_y);

    // Update position
    previous_mouse_position = cursor_pos;
  }
}

} // namespace lumina::platform::windows
