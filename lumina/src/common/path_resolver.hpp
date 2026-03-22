#pragma once

#include <filesystem>
#include <string_view>

namespace lumina::common {

class PathResolver {
public:
  explicit PathResolver(std::filesystem::path root) noexcept
      : root_(std::move(root)) {}

  [[nodiscard]] auto Resolve(std::string_view relative_path) const
      -> std::filesystem::path {
    return root_ / relative_path;
  }

  [[nodiscard]] auto Root() const noexcept -> const std::filesystem::path & {
    return root_;
  }

private:
  std::filesystem::path root_;
};

} // namespace lumina::common
