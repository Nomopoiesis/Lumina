#include "linux_window.hpp"

namespace lumina::platform::llinux {

auto Window::Create() -> std::expected<std::reference_wrapper<Window>,
                                       common::WindowCreationError> {
  auto &window = GetStaticInstance();
  window.connection = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(window.connection) != 0) {
    return std::unexpected(
        common::WindowCreationError{.message = "Failed to connect to XCB"});
  }

  // get screens
  const xcb_setup_t *setup = xcb_get_setup(window.connection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  xcb_screen_t *screen = iter.data;

  // create window
  xcb_window_t window_id = xcb_generate_id(window.connection);
  xcb_create_window(window.connection, XCB_COPY_FROM_PARENT, window_id,
                    screen->root, 0, 0, 800, 600, 10,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0,
                    NULL);

  // Map (show) the window and flush commands
  xcb_map_window(window.connection, window_id);
  xcb_flush(window.connection);

  return std::ref(window);
}

} // namespace lumina::platform::llinux