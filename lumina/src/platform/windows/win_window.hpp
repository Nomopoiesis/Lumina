#pragma once

#include "core/lumina_engine.hpp"
#include "lumina_types.hpp"

#include <expected>
#include <functional>
#include <stdexcept>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace lumina::platform::windows {

struct WindowCreationError {
  std::string message;
};

class Window {
public:
  static auto Instance() -> Window & {
    auto &instance = GetStaticInstance();
    if (!instance.is_initialized) {
      LOG_CRITICAL("Window not initialized, call Create() first");
      std::terminate();
    }
    return instance;
  }
  static auto
  Create(HINSTANCE instance, const char *title,
         u32 pos_x = DEFAULT_WINDOW_POS_X, u32 pos_y = DEFAULT_WINDOW_POS_Y,
         u32 width = DEFAULT_WINDOW_WIDTH, u32 height = DEFAULT_WINDOW_HEIGHT)
      -> std::expected<std::reference_wrapper<Window>, WindowCreationError>;
  static auto Destroy() -> void;
  [[nodiscard]] auto IsRunning() const -> bool;
  auto ProcessMessages() -> void;

  [[nodiscard]] auto GetWindowClientDimensions() const
      -> core::WindowDimensions;

  auto Close() -> void { is_running = false; }

  [[nodiscard]] auto GetWindowHandle() const -> HWND { return window_handle; }
  [[nodiscard]] auto GetHInstance() const -> HINSTANCE { return instance; }

private:
  Window() noexcept = default;

  static constexpr u32 DEFAULT_WINDOW_POS_X = 100;
  static constexpr u32 DEFAULT_WINDOW_POS_Y = 100;
  static constexpr u32 DEFAULT_WINDOW_WIDTH = 800;
  static constexpr u32 DEFAULT_WINDOW_HEIGHT = 600;
  static constexpr const char *DEFAULT_WINDOW_CLASS_NAME = "LuminaWindowClass";

  static auto GetStaticInstance() -> Window & {
    static Window instance;
    return instance;
  }

  HWND window_handle = nullptr;
  HINSTANCE instance = nullptr;

  bool is_running = false;

  bool is_initialized = false;
};

} // namespace lumina::platform::windows
