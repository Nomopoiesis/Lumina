#pragma once

#include "platform/platform_common/window_base.hpp"

#include <xcb/xcb.h>

#include <expected>

namespace lumina::platform::llinux {

class Window : public common::WindowBase<Window> {
public:
  static auto Create() -> std::expected<std::reference_wrapper<Window>,
                                        common::WindowCreationError>;

private:
  static auto GetStaticInstance() -> Window & {
    static Window instance;
    return instance;
  }

  xcb_connection_t *connection = nullptr;

  friend class common::WindowBase<Window>;
};

} // namespace lumina::platform::llinux