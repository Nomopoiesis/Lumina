#pragma once

#include <string>
#include <utility>

namespace lumina::common {

class LuminaError {
public:
  LuminaError() noexcept = default;
  LuminaError(std::string message) noexcept : message_(std::move(message)) {}

  [[nodiscard]] auto Message() const noexcept -> std::string_view {
    return message_;
  }

private:
  std::string message_;
};

} // namespace lumina::common
