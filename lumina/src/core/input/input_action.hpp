#pragma once

#include "lumina_types.hpp"

#include "input_state.hpp"

#include <string_view>
#include <unordered_map>
#include <variant>

namespace lumina::core {

struct ActionID {
  u64 id;
  consteval ActionID(const char *name) noexcept
      : id(std::hash<std::string_view>{}(name)) {}

  explicit constexpr ActionID(std::string_view name) noexcept
      : id(std::hash<std::string_view>{}(name)) {}

  auto operator==(const ActionID &other) const -> bool = default;
};

struct KeyBinding {
  KeyCode key_code;
  KeyState key_state;
};

struct MouseButtonBinding {
  MouseButton mouse_button;
  KeyState key_state;
};

struct MouseAxisBinding {
  MouseAxis mouse_axis;
};

using InputBinding =
    std::variant<KeyBinding, MouseButtonBinding, MouseAxisBinding>;

struct ActionEvent {
  ActionID action_id;
  KeyState key_state;
  f32 axis_value = 0.0F;
  bool consumed = false;
};

class InputActionMap {
public:
  auto BindAction(ActionID action_id, InputBinding binding) -> void;
  auto GatherActions(const InputState &input_state) -> std::vector<ActionEvent>;

  [[nodiscard]] auto GetActiveBindingState(const InputBinding &binding,
                                           const InputState &input_state) const
      -> std::pair<bool, KeyState>;

private:
  struct ActionIDHash {
    auto operator()(const ActionID &action_id) const -> std::size_t {
      return action_id.id;
    }
  };
  std::unordered_map<ActionID, InputBinding, ActionIDHash> action_bindings;
};

auto KeyInputBinding(KeyCode key_code, KeyState key_state) -> InputBinding;
auto MouseButtonInputBinding(MouseButton mouse_button, KeyState key_state)
    -> InputBinding;
// auto MouseAxisInputBinding(MouseAxis mouse_axis, KeyState key_state) ->
// InputBinding;

} // namespace lumina::core
