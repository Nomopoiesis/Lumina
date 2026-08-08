#pragma once

#include "lumina_assert.hpp"

#include <filesystem>
#include <format>
#include <string_view>

namespace lumina::common {

class PathResolver {
public:
  explicit PathResolver(std::filesystem::path root) noexcept
      : root_(std::move(root)) {}

  [[nodiscard]] auto Resolve(std::string_view relative_path) const
      -> std::filesystem::path {
    const std::filesystem::path input(relative_path);
    // path::operator/ replaces the left operand outright when the right one
    // carries a root name or root directory, so handing an absolute path to a
    // resolver would silently ignore root_ and return the input unchanged.
    // relative_path() drops any such root, keeping root_ authoritative.
    ASSERT(!(input.has_root_name() || input.has_root_directory()),
           std::format("PathResolver: expected a relative path, got '{}'",
                       relative_path));
    return root_ / input.relative_path();
  }

  [[nodiscard]] auto Root() const noexcept -> const std::filesystem::path & {
    return root_;
  }

private:
  std::filesystem::path root_;
};

} // namespace lumina::common
