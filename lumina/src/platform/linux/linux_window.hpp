#pragma once

#include "core/window_dimensions.hpp"
#include "platform/platform_common/window_base.hpp"

#include <xcb/xcb.h>

#include <expected>

namespace lumina::platform::llinux {

class Window : public common::WindowBase<Window> {
public:
  static auto Create(const char *title) -> std::expected<std::reference_wrapper<Window>,
                                                         common::WindowCreationError>;

  static auto Destroy() -> void;

  auto ProcessMessages() -> void;
  auto ProcessMouseMovement() -> void;

  [[nodiscard]] auto GetWindowClientDimensions() const
      -> core::WindowDimensions;

  [[nodiscard]] auto GetXCBWindowHandle() const -> xcb_window_t {
    return xcb_window_handle;
  }
  [[nodiscard]] auto GetXCBConnection() const -> xcb_connection_t * {
    return connection;
  }

private:
  static auto GetStaticInstance() -> Window & {
    static Window instance;
    return instance;
  }

  xcb_connection_t *connection = nullptr;
  xcb_window_t xcb_window_handle = 0;
  xcb_atom_t wm_delete_window_atom = XCB_ATOM_NONE;

  friend class common::WindowBase<Window>;
};

} // namespace lumina::platform::llinux