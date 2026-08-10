#pragma once

#include "common/lumina_error.hpp"
#include "common/path_registry.hpp"
#include "static_mesh.hpp"
#include "static_mesh_registry.hpp"

#include <expected>
#include <string_view>

namespace lumina::core {

using namespace lumina::common;

class StaticMeshManager {
public:
  StaticMeshManager() noexcept
      : model_cache_path_resolver(
            lumina::common::PathRegistry::Instance().model_cache) {}
  StaticMeshManager(const StaticMeshManager &) = delete;
  StaticMeshManager(StaticMeshManager &&) noexcept = delete;
  auto operator=(const StaticMeshManager &) -> StaticMeshManager & = delete;
  auto operator=(StaticMeshManager &&) noexcept -> StaticMeshManager & = delete;
  ~StaticMeshManager() = default;

  auto LoadMesh(const std::string_view &path)
      -> std::expected<StaticMeshHandle, LuminaError>;
  auto CreateMesh(std::string_view name, StaticMesh &&mesh)
      -> std::expected<StaticMeshHandle, LuminaError>;

  [[nodiscard]] auto Get(const StaticMeshHandle &handle)
      -> std::optional<const StaticMesh *>;

  [[nodiscard]] auto GetRegistry() -> StaticMeshResourceRegistry & {
    return static_mesh_registry;
  }

private:
  StaticMeshResourceRegistry static_mesh_registry;
  std::unordered_map<std::string, StaticMeshHandle> loaded_meshes;
  const common::PathResolver &model_cache_path_resolver;
};

} // namespace lumina::core
