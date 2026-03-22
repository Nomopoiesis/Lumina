#pragma once

#include "path_resolver.hpp"

#include <filesystem>

namespace lumina::common {

class PathRegistry {
public:
  PathRegistry(const PathRegistry &) = delete;
  auto operator=(const PathRegistry &) -> PathRegistry & = delete;
  PathRegistry(PathRegistry &&) noexcept = delete;
  auto operator=(PathRegistry &&) noexcept -> PathRegistry & = delete;

  static auto Initialize(std::filesystem::path runtime_root) -> void;
  static auto Instance() -> PathRegistry &;
  static auto Shutdown() -> void;

  const PathResolver shaders;
  const PathResolver textures;
  const PathResolver models;
  const PathResolver model_cache;

private:
  explicit PathRegistry(const std::filesystem::path &root)
      : shaders(root / "shaders"), textures(root / "textures"),
        models(root / "models"), model_cache(root / "model_cache") {}

  static PathRegistry *instance_;
};

} // namespace lumina::common
