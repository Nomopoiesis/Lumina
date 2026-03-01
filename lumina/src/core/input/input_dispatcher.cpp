#include "input_dispatcher.hpp"

namespace lumina::core {

auto InputDispatcher::RegisterHandler(IInputHandler *handler, u32 priority)
    -> void {
  handlers.emplace(priority, handler);
}

auto InputDispatcher::Dispatch(const InputState &input_state) -> void {
  auto actions = input_action_map.GatherActions(input_state);
  if (actions.empty()) {
    return;
  }
  auto handlers = this->handlers;
  while (!handlers.empty()) {
    auto [priority, handler] = handlers.top();
    handler->HandleInput(actions);
    handlers.pop();
  }
}

} // namespace lumina::core