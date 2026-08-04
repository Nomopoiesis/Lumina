#pragma once

#include "common/logger/logger.hpp"
#include "common/lumina_terminate.hpp"

namespace lumina::platform::common {

struct WindowCreationError {
  std::string message;
};

template <typename Derived>
class WindowBase {
public: // static methods
  static auto Instance() -> Derived & {
    auto &instance = Derived::GetStaticInstance();
    if (!instance.is_initialized) {
      LOG_CRITICAL("Window not initialized, call Create() first");
      LUMINA_TERMINATE();
    }
    return static_cast<Derived &>(instance);
  }

public:
  WindowBase(const WindowBase &) = delete;
  auto operator=(const WindowBase &) -> WindowBase & = delete;
  WindowBase(WindowBase &&) = delete;
  auto operator=(WindowBase &&) -> WindowBase & = delete;

  [[nodiscard]] auto IsRunning() const -> bool { return is_running; }
  auto Close() -> void { is_running = false; }

  auto SetMouseTrapped(bool trapped) -> void { mouse_trapped = trapped; }
  [[nodiscard]] auto IsMouseTrapped() const -> bool { return mouse_trapped; }

protected:
  WindowBase() noexcept = default;
  ~WindowBase() = default;

  bool is_initialized = false;
  bool is_running = false;
  bool mouse_trapped = false;
};

} // namespace lumina::platform::common