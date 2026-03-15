#pragma once

#include "lumina_types.hpp"

#include <utility>

namespace lumina::core {

enum class KeyCode : u8 {
  // Letters
  A = 0,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,

  // Numbers
  Num0,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,

  // Function keys
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,

  // Modifiers
  LeftShift,
  RightShift,
  LeftCtrl,
  RightCtrl,
  LeftAlt,
  RightAlt,

  // Special keys
  Space,
  Enter,
  Escape,
  Tab,
  Backspace,
  Delete,
  Insert,
  Home,
  End,
  PageUp,
  PageDown,

  // Arrows
  Left,
  Right,
  Up,
  Down,

  COUNT_
};

enum class MouseButton : u8 {
  Left = 0,
  Right,
  Middle,
  COUNT_,
};

enum class MouseAxis : u8 {
  X = 0,
  Y,
  COUNT_,
};

enum class KeyState : u8 {
  Released = 0,
  Pressed,
  Held,
};

// Keyboard input data
struct KeyInput {
  KeyState state[static_cast<u8>(KeyCode::COUNT_)] = {};
};

// Mouse input data
struct MouseInput {
  i32 x = 0, y = 0;               // Current position
  i32 delta_x = 0, delta_y = 0;   // Movement this frame
  i32 scroll_x = 0, scroll_y = 0; // Scroll wheel
  KeyState buttons[static_cast<u8>(MouseButton::COUNT_)] = {};
};

class InputState {
public:
  InputState() noexcept = default;
  ~InputState() = default;

  InputState(const InputState &) = delete;
  auto operator=(const InputState &) -> InputState & = delete;
  InputState(InputState &&) noexcept = default;
  auto operator=(InputState &&) noexcept -> InputState & = default;

  auto NewFrame() -> void;
  auto SetKeyState(KeyCode key, KeyState state) -> void;
  auto SetMouseButtonState(MouseButton button, KeyState state) -> void;
  auto SetMousePosition(i32 x, i32 y) -> void;
  auto SetMouseDelta(i32 delta_x, i32 delta_y) -> void;
  auto SetMouseScroll(i32 scroll_x, i32 scroll_y) -> void;

  [[nodiscard]] auto GetKeyState(KeyCode key) const -> KeyState;
  [[nodiscard]] auto GetMouseButtonState(MouseButton button) const -> KeyState;
  [[nodiscard]] auto GetMousePosition() const -> std::pair<i32, i32>;
  [[nodiscard]] auto GetMouseDelta() const -> std::pair<i32, i32>;
  [[nodiscard]] auto GetMouseScroll() const -> std::pair<i32, i32>;

private:
  KeyInput key_input{};
  MouseInput mouse_input{};
};

} // namespace lumina::core
