#pragma once

#include <functional>
#include <utility>

class ScopeGuard {
public:
  ScopeGuard(std::function<void()> on_exit) noexcept
      : on_exit_(std::move(on_exit)) {}

  ~ScopeGuard() {
    if (on_exit_) {
      on_exit_();
    }
  }

  // Prevent copying - scope guard should only execute cleanup once
  ScopeGuard(const ScopeGuard &) = delete;
  auto operator=(const ScopeGuard &) -> ScopeGuard & = delete;

  // Allow moving - moved-from object should not execute cleanup
  ScopeGuard(ScopeGuard &&other) noexcept
      : on_exit_(std::exchange(other.on_exit_, nullptr)) {}
  auto operator=(ScopeGuard &&other) noexcept -> ScopeGuard & {
    if (this != &other) {
      if (on_exit_) {
        on_exit_();
      }
      on_exit_ = std::exchange(other.on_exit_, nullptr);
    }
    return *this;
  }

private:
  std::function<void()> on_exit_;
};
