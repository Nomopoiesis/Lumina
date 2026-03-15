#pragma once

#include "input_action.hpp"

#include <queue>
#include <span>

namespace lumina::core {

class IInputHandler {
public:
  IInputHandler() noexcept = default;
  IInputHandler(const IInputHandler &) = delete;
  auto operator=(const IInputHandler &) -> IInputHandler & = delete;
  IInputHandler(IInputHandler &&) noexcept = default;
  auto operator=(IInputHandler &&) noexcept -> IInputHandler & = default;
  virtual ~IInputHandler() = default;

  virtual auto HandleInput(const std::span<const ActionEvent> &action_events)
      -> void = 0;
};

class InputDispatcher {
public:
  InputDispatcher() noexcept = default;
  ~InputDispatcher() noexcept = default;

  InputDispatcher(const InputDispatcher &) = delete;
  auto operator=(const InputDispatcher &) -> InputDispatcher & = delete;
  InputDispatcher(InputDispatcher &&) noexcept = default;
  auto operator=(InputDispatcher &&) noexcept -> InputDispatcher & = default;

  auto GetInputActionMap() -> InputActionMap & { return input_action_map; }

  auto RegisterHandler(IInputHandler *handler, u32 priority) -> void;

  auto Dispatch(const InputState &input_state) -> void;

private:
  InputActionMap input_action_map;
  std::priority_queue<std::pair<u32, IInputHandler *>> handlers;
};

} // namespace lumina::core
