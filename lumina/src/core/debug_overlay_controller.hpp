#pragma once

#include "input/input_dispatcher.hpp"

namespace lumina::core {

class DebugOverlayController : public IInputHandler {
public:
  auto HandleInput(const std::span<const ActionEvent> &action_events)
      -> void override;
};

} // namespace lumina::core