#include "mesh_cache.hpp"

#include "common/data_structures/data_buffer.hpp"
#include "common/logger/logger.hpp"
#include "common/scope_guard.hpp"
#include "platform/platform_common/file_handle.hpp"

#include <filesystem>
#include <format>
#include <string>

namespace lumina::core {

using lumina::platform::common::FileHandle;
using lumina::platform::common::InvalidFileHandle;

// Binary format (little-endian):
//   [4]  magic: 'L','M','S','H'
//   [4]  version: u32 = 1
//   [8]  vertex_count: u64
//   [4]  attribute_count: u32
//   [4]  index_count: u32
//   per attribute:
//     [1]  VertexAttributeType (u8)
//     [1]  ElementType (u8)
//     [8]  data_byte_count: u64
//     [N]  raw attribute bytes
//   [12] Vec3[float, float, float] - bounding box min
//   [12] Vec3[float, float, float] - bounding box max
//   [index_count * 2]  u16 index array

namespace {

constexpr u32 Magic = 0x48534D4C; // 'L','M','S','H' in little-endian
constexpr u32 Version = 1;

auto Fnv1a64(std::string_view text) -> u64 {
  constexpr u64 OffsetBasis = 14695981039346656037ULL;
  constexpr u64 Prime = 1099511628211ULL;

  u64 hash = OffsetBasis;
  for (const char character : text) {
    hash ^= static_cast<u8>(character);
    hash *= Prime;
  }
  return hash;
}

// Cache keys are asset paths, so they cannot be used as a path suffix: an
// absolute key would make path::operator/ drop the cache root entirely, and
// even a relative one would scatter .lmesh files across the source tree. Hash
// the key into a single flat filename, prefixed with the stem so the cache
// directory stays readable.
auto CachePath(std::string_view cache_key,
               const common::PathResolver &cache_resolver)
    -> std::filesystem::path {
  const auto stem = std::filesystem::path(cache_key).stem().string();
  return cache_resolver.Resolve(
      std::format("{}_{:016x}.lmesh", stem, Fnv1a64(cache_key)));
}

// True when the source asset has been written since the cache entry was
// produced. If either timestamp is unreadable — the source has been moved away,
// say — the entry counts as fresh: a cached mesh we cannot compare against
// still beats discarding the only copy we have.
auto IsCacheStale(std::string_view source_path,
                  const std::filesystem::path &cache_path) -> bool {
  auto &platform_services = platform::common::PlatformServices::Instance();

  u64 source_write_time = 0;
  if (!platform_services.LuminaGetFileWriteTime(std::string(source_path).c_str(),
                                                &source_write_time)) {
    return false;
  }

  u64 cache_write_time = 0;
  if (!platform_services.LuminaGetFileWriteTime(cache_path.string().c_str(),
                                                &cache_write_time)) {
    return false;
  }

  return source_write_time > cache_write_time;
}

} // namespace

auto SerializeStaticMesh(const StaticMesh &mesh, std::string_view cache_key,
                         const common::PathResolver &cache_resolver)
    -> std::expected<void, MeshCacheError> {
  const auto path = CachePath(cache_key, cache_resolver);

  auto create_directory_success =
      platform::common::PlatformServices::Instance().LuminaCreateDirectory(
          path.parent_path().string().c_str());
  if (!create_directory_success) {
    LOG_ERROR("mesh_cache: failed to create cache directory: {}",
              path.parent_path().string());
    return std::unexpected(MeshCacheError{"failed to create cache directory"});
  }

  // LuminaCreateFile opens for append, so an entry left over from a previous
  // run would be extended rather than replaced. Drop it first: rewriting an
  // existing entry is the normal path whenever a source asset changes.
  platform::common::PlatformServices::Instance().LuminaDeleteFile(
      path.string().c_str());

  auto file_handle =
      platform::common::PlatformServices::Instance().LuminaCreateFile(
          path.string().c_str());
  if (file_handle == InvalidFileHandle) {
    LOG_ERROR("mesh_cache: failed to open file for writing: {}", path.string());
    return std::unexpected(
        MeshCacheError{"failed to open cache file for writing"});
  }
  ScopeGuard close_file([file_handle]() -> void {
    platform::common::PlatformServices::Instance().LuminaCloseFile(file_handle);
  });

  std::vector<u8> data_to_write;
  auto write = [&data_to_write](const void *data, size_t size) -> void {
    data_to_write.insert(data_to_write.end(), static_cast<const u8 *>(data),
                         static_cast<const u8 *>(data) + size);
  };

  write(&Magic, sizeof(Magic));
  write(&Version, sizeof(Version));
  write(&mesh.vertex_count, sizeof(mesh.vertex_count));

  const auto attribute_count = static_cast<u32>(mesh.vertex_attributes.size());
  write(&attribute_count, sizeof(attribute_count));

  const auto index_count = static_cast<u32>(mesh.indices.size());
  write(&index_count, sizeof(index_count));

  for (const auto &[attr, data] : mesh.vertex_attributes) {
    const auto attr_type = static_cast<u8>(attr.type);
    const auto elem_type = static_cast<u8>(attr.element_type);
    const auto byte_count = static_cast<u64>(data.Size());
    write(&attr_type, sizeof(attr_type));
    write(&elem_type, sizeof(elem_type));
    write(&byte_count, sizeof(byte_count));
    write(data.Data(), byte_count);
  }

  // AABB is stored explicitly so loaders don't need to scan vertex positions.
  static_assert(sizeof(math::Vec3) == 12,
                "AABB cache layout expects packed Vec3");
  write(&mesh.bounding_box.min, sizeof(mesh.bounding_box.min));
  write(&mesh.bounding_box.max, sizeof(mesh.bounding_box.max));

  write(mesh.indices.data(), mesh.indices.size() * sizeof(u16));

  auto write_success =
      platform::common::PlatformServices::Instance().LuminaWriteFile(
          file_handle, data_to_write.data(), data_to_write.size());
  if (!write_success) {
    LOG_ERROR("mesh_cache: write error for: {}", path.string());
    return std::unexpected(MeshCacheError{"write error"});
  }

  LOG_INFO("mesh_cache: wrote {}", path.string());
  return {};
}

auto HasCachedMesh(std::string_view cache_key,
                   const common::PathResolver &cache_resolver) -> bool {
  auto path = CachePath(cache_key, cache_resolver);
  if (!std::filesystem::exists(path)) {
    return false;
  }

  if (!IsCacheStale(cache_key, path)) {
    return true;
  }

  // Report a miss so the caller re-parses the source, and drop the entry now
  // rather than leaving a file that reads as a valid cache of an older mesh.
  LOG_INFO("mesh_cache: {} changed since it was cached, invalidating {}",
           cache_key, path.string());
  if (!platform::common::PlatformServices::Instance().LuminaDeleteFile(
          path.string().c_str())) {
    LOG_WARNING("mesh_cache: failed to delete stale cache file: {}",
                path.string());
  }
  return false;
}

auto DeserializeStaticMesh(std::string_view cache_key,
                           const common::PathResolver &cache_resolver)
    -> std::expected<StaticMesh, MeshCacheError> {
  const auto path = CachePath(cache_key, cache_resolver);

  auto file_handle =
      platform::common::PlatformServices::Instance().LuminaOpenFile(
          path.string().c_str());
  if (file_handle == InvalidFileHandle) {
    LOG_ERROR("mesh_cache: failed to open cache file: {}", path.string());
    return std::unexpected(MeshCacheError{"failed to open cache file"});
  }
  ScopeGuard close_file([file_handle]() -> void {
    platform::common::PlatformServices::Instance().LuminaCloseFile(file_handle);
  });

  auto file_size =
      platform::common::PlatformServices::Instance().LuminaGetFileSize(
          file_handle);
  if (file_size == 0) {
    LOG_ERROR("mesh_cache: failed to get file size: {}", path.string());
    return std::unexpected(MeshCacheError{"failed to get file size"});
  }

  common::data_structures::DataBuffer data_to_read(file_size);
  auto read_success =
      platform::common::PlatformServices::Instance().LuminaReadFile(
          file_handle, data_to_read.Data(), file_size);
  if (!read_success) {
    LOG_ERROR("mesh_cache: failed to read file: {}", path.string());
    return std::unexpected(MeshCacheError{"failed to read file"});
  }

  size_t offset = 0;
  u32 magic = 0;
  u32 version = 0;
  magic = data_to_read.As<u32>(offset);
  offset += sizeof(u32);
  version = data_to_read.As<u32>(offset);
  offset += sizeof(u32);

  if (magic != Magic) {
    LOG_ERROR("mesh_cache: invalid magic in {}", path.string());
    return std::unexpected(MeshCacheError{"invalid cache file magic"});
  }
  if (version != Version) {
    LOG_WARNING("mesh_cache: version mismatch in {} (got {}, expected {}) — "
                "treating as cache miss",
                path.string(), version, Version);
    return std::unexpected(MeshCacheError{"cache file version mismatch"});
  }

  StaticMesh mesh;

  mesh.vertex_count = data_to_read.As<u64>(offset);
  offset += sizeof(u64);

  u32 attribute_count = 0;
  attribute_count = data_to_read.As<u32>(offset);
  offset += sizeof(u32);
  u32 index_count = data_to_read.As<u32>(offset);
  offset += sizeof(u32);

  mesh.vertex_attributes.reserve(attribute_count);
  for (u32 i = 0; i < attribute_count; ++i) {
    u8 attr_type = 0;
    u8 elem_type = 0;
    u64 byte_count = 0;
    attr_type = data_to_read.As<u8>(offset);
    offset += sizeof(u8);
    elem_type = data_to_read.As<u8>(offset);
    offset += sizeof(u8);
    byte_count = data_to_read.As<u64>(offset);
    offset += sizeof(u64);

    DataBuffer data(byte_count);
    std::memcpy(data.Data(), data_to_read.Data() + offset, byte_count);
    offset += byte_count;

    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = static_cast<VertexAttributeType>(attr_type),
                        .element_type = static_cast<ElementType>(elem_type)},
        std::move(data));
  }

  mesh.bounding_box.min = data_to_read.As<math::Vec3>(offset);
  offset += sizeof(math::Vec3);
  mesh.bounding_box.max = data_to_read.As<math::Vec3>(offset);
  offset += sizeof(math::Vec3);

  mesh.indices.resize(index_count);
  std::memcpy(mesh.indices.data(), data_to_read.Data() + offset,
              index_count * sizeof(u16));
  offset += index_count * sizeof(u16);

  LOG_INFO("mesh_cache: loaded {}", path.string());
  return mesh;
}

} // namespace lumina::core
