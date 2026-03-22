#include "mesh_cache.hpp"

#include "common/data_structures/data_buffer.hpp"
#include "common/logger/logger.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace lumina::core {

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
//   [index_count * 2]  u16 index array

namespace {

constexpr u32 Magic = 0x48534D4C; // 'L','M','S','H' in little-endian
constexpr u32 Version = 1;

auto CachePath(std::string_view cache_key,
               const common::PathResolver &cache_resolver)
    -> std::filesystem::path {
  return cache_resolver.Resolve(std::string(cache_key) + ".lmesh");
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
    LOG_ERROR("mesh_cache: failed to create cache directory: %s",
              path.parent_path().string().c_str());
    return std::unexpected(MeshCacheError{"failed to create cache directory"});
  }

  auto *file_handle =
      platform::common::PlatformServices::Instance().LuminaCreateFile(
          path.string().c_str());
  if (file_handle == nullptr) {
    LOG_ERROR("mesh_cache: failed to open file for writing: %s",
              path.string().c_str());
    return std::unexpected(
        MeshCacheError{"failed to open cache file for writing"});
  }

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
    const auto byte_count = static_cast<u64>(data.size());
    write(&attr_type, sizeof(attr_type));
    write(&elem_type, sizeof(elem_type));
    write(&byte_count, sizeof(byte_count));
    write(data.data(), static_cast<std::streamsize>(byte_count));
  }

  write(mesh.indices.data(),
        static_cast<std::streamsize>(mesh.indices.size() * sizeof(u16)));

  auto write_success =
      platform::common::PlatformServices::Instance().LuminaWriteFile(
          file_handle, data_to_write.data(), data_to_write.size());
  if (!write_success) {
    LOG_ERROR("mesh_cache: write error for: %s", path.string().c_str());
    return std::unexpected(MeshCacheError{"write error"});
  }

  LOG_INFO("mesh_cache: wrote {}", path.string().c_str());
  return {};
}

auto HasCachedMesh(std::string_view cache_key,
                   const common::PathResolver &cache_resolver) -> bool {
  return std::filesystem::exists(CachePath(cache_key, cache_resolver));
}

auto DeserializeStaticMesh(std::string_view cache_key,
                           const common::PathResolver &cache_resolver)
    -> std::expected<StaticMesh, MeshCacheError> {
  const auto path = CachePath(cache_key, cache_resolver);

  auto *file_handle =
      platform::common::PlatformServices::Instance().LuminaOpenFile(
          path.string().c_str());
  if (file_handle == nullptr) {
    LOG_ERROR("mesh_cache: failed to open cache file: %s",
              path.string().c_str());
    return std::unexpected(MeshCacheError{"failed to open cache file"});
  }

  auto file_size =
      platform::common::PlatformServices::Instance().LuminaGetFileSize(
          file_handle);
  if (file_size == 0) {
    LOG_ERROR("mesh_cache: failed to get file size: %s", path.string().c_str());
    return std::unexpected(MeshCacheError{"failed to get file size"});
  }

  common::data_structures::DataBuffer data_to_read(file_size);
  auto read_success =
      platform::common::PlatformServices::Instance().LuminaReadFile(
          file_handle, data_to_read.Data(), file_size);
  if (!read_success) {
    LOG_ERROR("mesh_cache: failed to read file: %s", path.string().c_str());
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
    LOG_ERROR("mesh_cache: invalid magic in %s", path.string().c_str());
    return std::unexpected(MeshCacheError{"invalid cache file magic"});
  }
  if (version != Version) {
    LOG_WARNING("mesh_cache: version mismatch in %s (got %u, expected %u) — "
                "treating as cache miss",
                path.string().c_str(), version, Version);
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

    std::vector<u8> data(byte_count);
    std::memcpy(data.data(), data_to_read.Data() + offset, byte_count);
    offset += byte_count;

    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = static_cast<VertexAttributeType>(attr_type),
                        .element_type = static_cast<ElementType>(elem_type)},
        std::move(data));
  }

  mesh.indices.resize(index_count);
  std::memcpy(mesh.indices.data(), data_to_read.Data() + offset,
              index_count * sizeof(u16));
  offset += index_count * sizeof(u16);

  LOG_INFO("mesh_cache: loaded {}", path.string().c_str());
  return mesh;
}

} // namespace lumina::core
