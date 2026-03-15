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
  auto local_handlers = this->handlers;
  while (!local_handlers.empty()) {
    auto [priority, handler] = local_handlers.top();
    handler->HandleInput(actions);
    local_handlers.pop();
  }
}

} // namespace lumina::core
