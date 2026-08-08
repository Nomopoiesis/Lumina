#pragma once

#include "common/path_resolver.hpp"
#include "static_mesh.hpp"

#include <expected>
#include <string_view>

namespace lumina::core {

struct MeshCacheError {
  const char *message;
};

// cache_key is the path of the source asset the mesh was built from. It is
// hashed into a flat filename under cache_resolver.Root(), and its
// last-modification time is what decides whether a cache entry is still valid.

// Serialize mesh to a .lmesh file for cache_key, replacing any existing entry.
// Creates the cache directory if it does not exist.
auto SerializeStaticMesh(const StaticMesh &mesh, std::string_view cache_key,
                         const common::PathResolver &cache_resolver)
    -> std::expected<void, MeshCacheError>;

// Returns true if a usable .lmesh file exists for cache_key. An entry older
// than the source asset is stale: it is deleted and reported as a miss, so the
// caller re-parses the source and writes a fresh entry.
[[nodiscard]] auto HasCachedMesh(std::string_view cache_key,
                                 const common::PathResolver &cache_resolver)
    -> bool;

// Deserialize a .lmesh file into a new StaticMesh.
[[nodiscard]] auto DeserializeStaticMesh(
    std::string_view cache_key, const common::PathResolver &cache_resolver)
    -> std::expected<StaticMesh, MeshCacheError>;

} // namespace lumina::core
