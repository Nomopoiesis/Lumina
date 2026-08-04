#include "linux_window.hpp"

#include "linux_input_map.hpp"

#include "core/lumina_engine.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>

namespace lumina::platform::llinux {

namespace {

struct XcbFree {
  auto operator()(void *p) const noexcept -> void {
    std::free(p); // NOLINT(cppcoreguidelines-no-malloc,hicpp-no-malloc)
  }
};

template <typename T>
using XcbPtr = std::unique_ptr<T, XcbFree>;

auto HandleMouseButton(xcb_button_t detail, core::KeyState state) -> void {
  core::MouseButton button{};
  if (detail == 1) {
    button = core::MouseButton::Left;
  } else if (detail == 2) {
    button = core::MouseButton::Middle;
  } else if (detail == 3) {
    button = core::MouseButton::Right;
  } else {
    return;
  }
  core::LuminaEngine::Instance().GetInputState().SetMouseButtonState(button,
                                                                     state);
}

auto HandleKeyPress(xcb_keycode_t keycode) -> void {
  const auto key_code = XCB_TO_KEY_CODE[keycode];
  if (key_code == UNMAPPED_KEY) {
    return;
  }

  auto &input = core::LuminaEngine::Instance().GetInputState();
  const auto current = input.GetKeyState(key_code);
  const bool was_down =
      (current == core::KeyState::Pressed || current == core::KeyState::Held);
  input.SetKeyState(key_code,
                    was_down ? core::KeyState::Held : core::KeyState::Pressed);
}

} // namespace

auto Window::Create(const char *title)
    -> std::expected<std::reference_wrapper<Window>,
                     common::WindowCreationError> {
  auto &window = GetStaticInstance();
  window.connection = xcb_connect(nullptr, nullptr);
  if (xcb_connection_has_error(window.connection) != 0) {
    return std::unexpected(
        common::WindowCreationError{.message = "Failed to connect to XCB"});
  }

  // get screens
  const xcb_setup_t *setup = xcb_get_setup(window.connection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  xcb_screen_t *screen = iter.data;
  u32 value_mask = XCB_CW_EVENT_MASK;
  u32 value_list[] = {XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                      XCB_EVENT_MASK_BUTTON_PRESS |
                      XCB_EVENT_MASK_BUTTON_RELEASE};

  // create window
  xcb_window_t window_id = xcb_generate_id(window.connection);
  xcb_create_window(window.connection, XCB_COPY_FROM_PARENT, window_id,
                    screen->root, 0, 0, 800, 600, 10,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                    value_mask, value_list);

  window.xcb_window_handle = window_id;

  // Register WM_DELETE_WINDOW so the close button sends a client message
  // instead of abruptly killing the connection
  xcb_intern_atom_cookie_t wm_protocols_cookie =
      xcb_intern_atom(window.connection, 1, 12, "WM_PROTOCOLS");
  xcb_intern_atom_cookie_t wm_delete_cookie =
      xcb_intern_atom(window.connection, 0, 16, "WM_DELETE_WINDOW");

  XcbPtr<xcb_intern_atom_reply_t> wm_protocols_reply{
      xcb_intern_atom_reply(window.connection, wm_protocols_cookie, nullptr)};
  XcbPtr<xcb_intern_atom_reply_t> wm_delete_reply{
      xcb_intern_atom_reply(window.connection, wm_delete_cookie, nullptr)};

  if ((wm_protocols_reply != nullptr) && (wm_delete_reply != nullptr)) {
    window.wm_delete_window_atom = wm_delete_reply->atom;
    xcb_change_property(window.connection, XCB_PROP_MODE_REPLACE, window_id,
                        wm_protocols_reply->atom, XCB_ATOM_ATOM, 32, 1,
                        &wm_delete_reply->atom);
  }

  // Set window title
  xcb_change_property(window.connection, XCB_PROP_MODE_REPLACE, window_id,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                      static_cast<u32>(std::strlen(title)), title);

  // Map (show) the window and flush commands
  xcb_map_window(window.connection, window_id);
  xcb_flush(window.connection);

  window.is_initialized = true;
  window.is_running = true;

  return std::ref(window);
}

auto Window::GetWindowClientDimensions() const -> core::WindowDimensions {
  xcb_get_geometry_cookie_t cookie =
      xcb_get_geometry(connection, xcb_window_handle);
  XcbPtr<xcb_get_geometry_reply_t> reply{
      xcb_get_geometry_reply(connection, cookie, nullptr)};
  if (reply == nullptr) {
    return {};
  }
  return {.width = reply->width, .height = reply->height};
}

auto Window::ProcessMessages() -> void {
  auto &engine = core::LuminaEngine::Instance();
  XcbPtr<xcb_generic_event_t> event{xcb_poll_for_event(connection)};
  while (event != nullptr) {
    switch (event->response_type & ~0x80) {
      case XCB_CLIENT_MESSAGE: {
        auto *cm = reinterpret_cast<xcb_client_message_event_t *>(event.get());
        if (cm->data.data32[0] == wm_delete_window_atom) {
          Close();
        }
      } break;
      case XCB_DESTROY_NOTIFY: {
        Close();
      } break;
      case XCB_KEY_PRESS: {
        auto *kp = reinterpret_cast<xcb_key_press_event_t *>(event.get());
        HandleKeyPress(kp->detail);
      } break;
      case XCB_BUTTON_PRESS: {
        auto *bp = reinterpret_cast<xcb_button_press_event_t *>(event.get());
        HandleMouseButton(bp->detail, core::KeyState::Pressed);
      } break;
      case XCB_BUTTON_RELEASE: {
        auto *br = reinterpret_cast<xcb_button_release_event_t *>(event.get());
        HandleMouseButton(br->detail, core::KeyState::Released);
      } break;
      case XCB_KEY_RELEASE: {
        auto *kr = reinterpret_cast<xcb_key_release_event_t *>(event.get());
        // Detect XCB auto-repeat: it fires KEY_RELEASE + KEY_PRESS back-to-back
        // for the same key at the same timestamp. Peek at the next event; if it
        // matches, discard both and keep the key in its current Held state.
        XcbPtr<xcb_generic_event_t> next{xcb_poll_for_event(connection)};
        const bool is_auto_repeat =
            (next != nullptr) &&
            ((next->response_type & ~0x80) == XCB_KEY_PRESS) &&
            (reinterpret_cast<xcb_key_press_event_t *>(next.get())->detail ==
             kr->detail) &&
            (reinterpret_cast<xcb_key_press_event_t *>(next.get())->time ==
             kr->time);
        if (is_auto_repeat) {
          // Both event and next freed by unique_ptr on scope exit / next
          // iteration
          event.reset(xcb_poll_for_event(connection));
          continue;
        }
        // Genuine release
        const auto key_code = XCB_TO_KEY_CODE[kr->detail];
        if (key_code != UNMAPPED_KEY) {
          engine.GetInputState().SetKeyState(key_code,
                                             core::KeyState::Released);
        }
        // xcb has no unget — process the peeked event inline
        if (next != nullptr) {
          if ((next->response_type & ~0x80) == XCB_KEY_PRESS) {
            HandleKeyPress(
                reinterpret_cast<xcb_key_press_event_t *>(next.get())->detail);
          }
        }
      } break;
      default:
        break;
    }
    event.reset(xcb_poll_for_event(connection));
  }
}

auto Window::ProcessMouseMovement() -> void {
  auto &input = core::LuminaEngine::Instance().GetInputState();

  xcb_query_pointer_cookie_t cookie =
      xcb_query_pointer(connection, xcb_window_handle);
  XcbPtr<xcb_query_pointer_reply_t> reply{
      xcb_query_pointer_reply(connection, cookie, nullptr)};
  if (reply == nullptr) {
    return;
  }

  const i32 cursor_x = reply->win_x;
  const i32 cursor_y = reply->win_y;

  if (mouse_trapped) {
    const auto dims = GetWindowClientDimensions();
    const i32 center_x = static_cast<i32>(dims.width) / 2;
    const i32 center_y = static_cast<i32>(dims.height) / 2;

    input.SetMouseDelta(cursor_x - center_x, cursor_y - center_y);
    input.SetMousePosition(center_x, center_y);

    // Recenter cursor for next frame
    xcb_warp_pointer(connection, XCB_NONE, xcb_window_handle, 0, 0, 0, 0,
                     static_cast<i16>(center_x), static_cast<i16>(center_y));
    xcb_flush(connection);
  } else {
    const auto [prev_x, prev_y] = input.GetMousePosition();
    input.SetMouseDelta(cursor_x - prev_x, cursor_y - prev_y);
    input.SetMousePosition(cursor_x, cursor_y);
  }
}

auto Window::Destroy() -> void {
  auto &window = GetStaticInstance();
  if (window.connection != nullptr) {
    xcb_destroy_window(window.connection, window.xcb_window_handle);
    xcb_disconnect(window.connection);
    window.connection = nullptr;
    window.xcb_window_handle = 0;
  }
}

} // namespace lumina::platform::llinux
