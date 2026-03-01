#include "input_state.hpp"

namespace lumina::core {

auto InputState::NewFrame() -> void {
  // itarate trough key codes and set pressed keys to held and released keys to
  // released
  for (auto &key : key_input.state) {
    if (key == KeyState::Pressed) {
      key = KeyState::Held;
    } else if (key == KeyState::Released) {
      key = KeyState::Released;
    }
  }
  // itarate trough mouse buttons and set pressed buttons to held and released
  // buttons to released
  for (auto &button : mouse_input.buttons) {
    if (button == KeyState::Pressed) {
      button = KeyState::Held;
    } else if (button == KeyState::Released) {
      button = KeyState::Released;
    }
  }

  mouse_input.delta_x = 0;
  mouse_input.delta_y = 0;
  mouse_input.scroll_x = 0;
  mouse_input.scroll_y = 0;
}

auto InputState::SetKeyState(KeyCode key, KeyState state) -> void {
  key_input.state[static_cast<u8>(key)] = state;
}

auto InputState::SetMouseButtonState(MouseButton button, KeyState state)
    -> void {
  mouse_input.buttons[static_cast<u8>(button)] = state;
}

auto InputState::SetMousePosition(i32 x, i32 y) -> void {
  mouse_input.x = x;
  mouse_input.y = y;
}

auto InputState::SetMouseDelta(i32 delta_x, i32 delta_y) -> void {
  mouse_input.delta_x = delta_x;
  mouse_input.delta_y = delta_y;
}

[[nodiscard]] auto InputState::GetKeyState(KeyCode key) const -> KeyState {
  return key_input.state[static_cast<u8>(key)];
}

[[nodiscard]] auto InputState::GetMouseButtonState(MouseButton button) const
    -> KeyState {
  return mouse_input.buttons[static_cast<u8>(button)];
}

[[nodiscard]] auto InputState::GetMousePosition() const -> std::pair<i32, i32> {
  return {mouse_input.x, mouse_input.y};
}

[[nodiscard]] auto InputState::GetMouseDelta() const -> std::pair<i32, i32> {
  return {mouse_input.delta_x, mouse_input.delta_y};
}

auto InputState::SetMouseScroll(i32 scroll_x, i32 scroll_y) -> void {
  mouse_input.scroll_x += scroll_x;
  mouse_input.scroll_y += scroll_y;
}

[[nodiscard]] auto InputState::GetMouseScroll() const -> std::pair<i32, i32> {
  return {mouse_input.scroll_x, mouse_input.scroll_y};
}
} // namespace lumina::core