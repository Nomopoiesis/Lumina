#pragma once

#include "common/path_resolver.hpp"
#include "static_mesh.hpp"

#include <expected>
#include <string_view>

namespace lumina::core {

struct MeshCacheError {
  const char *message;
};

// Serialize mesh to <cache_resolver.Root()>/<cache_key>.lmesh.
// Creates the cache directory if it does not exist.
auto SerializeStaticMesh(const StaticMesh &mesh, std::string_view cache_key,
                         const common::PathResolver &cache_resolver)
    -> std::expected<void, MeshCacheError>;

// Returns true if a .lmesh file exists for cache_key.
[[nodiscard]] auto HasCachedMesh(std::string_view cache_key,
                                 const common::PathResolver &cache_resolver)
    -> bool;

// Deserialize a .lmesh file into a new StaticMesh.
[[nodiscard]] auto DeserializeStaticMesh(
    std::string_view cache_key, const common::PathResolver &cache_resolver)
    -> std::expected<StaticMesh, MeshCacheError>;

} // namespace lumina::core
