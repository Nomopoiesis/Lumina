#include "input_action.hpp"

namespace lumina::core {

auto InputActionMap::BindAction(ActionID action_id, InputBinding binding)
    -> void {
  action_bindings[action_id] = binding;
}

auto InputActionMap::GatherActions(const InputState &input_state)
    -> std::vector<ActionEvent> {
  std::vector<ActionEvent> actions;
  for (const auto &[action_id, binding] : action_bindings) {
    const auto [is_active, key_state] =
        GetActiveBindingState(binding, input_state);
    if (is_active) {
      const auto [x, y] = input_state.GetMouseDelta();
      f32 axis_value = 0.0F;
      std::visit(
          [&](auto &&b) -> void {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, MouseAxisBinding>) {
              if (b.mouse_axis == MouseAxis::X) {
                axis_value = static_cast<f32>(x);
              } else if (b.mouse_axis == MouseAxis::Y) {
                axis_value = static_cast<f32>(y);
              }
            }
          },
          binding);
      actions.push_back(ActionEvent{
          .action_id = action_id,
          .key_state = key_state,
          .axis_value = axis_value,
      });
    }
  }
  return actions;
}

auto InputActionMap::GetActiveBindingState(const InputBinding &binding,
                                           const InputState &input_state) const
    -> std::pair<bool, KeyState> {
  return std::visit(
      [&](auto &&b) -> std::pair<bool, KeyState> {
        using T = std::decay_t<decltype(b)>;
        if constexpr (std::is_same_v<T, KeyBinding>) {
          return {input_state.GetKeyState(b.key_code) == b.key_state,
                  input_state.GetKeyState(b.key_code)};
        } else if constexpr (std::is_same_v<T, MouseButtonBinding>) {
          return {input_state.GetMouseButtonState(b.mouse_button) ==
                      b.key_state,
                  input_state.GetMouseButtonState(b.mouse_button)};
        } else if constexpr (std::is_same_v<T, MouseAxisBinding>) {
          return {
              true,
              KeyState::Released,
          };
        } else {
          static_assert(std::is_same_v<T, KeyBinding> ||
                            std::is_same_v<T, MouseButtonBinding> ||
                            std::is_same_v<T, MouseAxisBinding>,
                        "Invalid binding type");
          return {false, KeyState::Released};
        }
      },
      binding);
}

auto KeyInputBinding(KeyCode key_code, KeyState key_state) -> InputBinding {
  return KeyBinding{.key_code = key_code, .key_state = key_state};
}

auto MouseButtonInputBinding(MouseButton mouse_button, KeyState key_state)
    -> InputBinding {
  return MouseButtonBinding{.mouse_button = mouse_button,
                            .key_state = key_state};
}

auto MouseAxisInputBinding(MouseAxis mouse_axis) -> InputBinding {
  return MouseAxisBinding{.mouse_axis = mouse_axis};
}

} // namespace lumina::core
