#include "static_mesh_manager.hpp"

#include "common/logger/logger.hpp"
#include "data_parsers/obj_parser.hpp"
#include "mesh_cache.hpp"
#include "platform/platform_common/platform_services.hpp"

namespace lumina::core {

auto StaticMeshManager::LoadMesh(const std::string_view &path)
    -> std::expected<StaticMeshHandle, LuminaError> {
  // Check if the mesh is already loaded
  auto it = loaded_meshes.find(std::string(path));
  if (it != loaded_meshes.end()) {
    return it->second;
  }

  // Check if the mesh cached (serilized to disk)
  bool loaded_from_cache = false;
  StaticMesh mesh;
  if (HasCachedMesh(path, model_cache_path_resolver)) {
    // Load the mesh from disk
    auto deserialize_result =
        DeserializeStaticMesh(path, model_cache_path_resolver);
    if (deserialize_result.has_value()) {
      mesh = std::move(*deserialize_result);
      loaded_from_cache = true;
    }
  }

  if (!loaded_from_cache) {
    // std::string_view is not guaranteed null-terminated and LuminaOpenFile
    // takes a C string, so this cannot pass path.data() straight through.
    const std::string source_path(path);

    // Load the mesh from disk
    auto file_handle =
        platform::common::PlatformServices::Instance().LuminaOpenFile(
            source_path.c_str());
    if (file_handle == platform::common::InvalidFileHandle) {
      return std::unexpected(
          LuminaError(std::format("Failed to open model file: {}", path)));
    }

    std::size_t file_size =
        platform::common::PlatformServices::Instance().LuminaGetFileSize(
            file_handle);
    if (file_size == 0) {
      return std::unexpected(
          LuminaError(std::format("Failed to get file size: {}", path)));
    }

    common::data_structures::DataBuffer data_buffer(file_size);
    const bool read_success =
        platform::common::PlatformServices::Instance().LuminaReadFile(
            file_handle, data_buffer.Data(), file_size);
    platform::common::PlatformServices::Instance().LuminaCloseFile(file_handle);
    if (!read_success) {
      return std::unexpected(
          LuminaError(std::format("Failed to read model file: {}", path)));
    }

    auto obj_data = data_parsers::ParseOBJ(data_buffer.View());
    if (obj_data.vertex_count == 0) {
      return std::unexpected(LuminaError(
          std::format("Model file contained no geometry: {}", path)));
    }
    mesh.vertex_count = obj_data.vertex_count;
    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::Position,
                        .element_type = ElementType::Vec3},
        DataBuffer(reinterpret_cast<u8 *>(obj_data.positions.data()),
                   obj_data.positions.size() * sizeof(math::Vec3)));
    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::Normal,
                        .element_type = ElementType::Vec3},
        DataBuffer(reinterpret_cast<u8 *>(obj_data.normals.data()),
                   obj_data.normals.size() * sizeof(math::Vec3)));
    mesh.vertex_attributes.emplace_back(
        VertexAttribute{.type = VertexAttributeType::TexCoord,
                        .element_type = ElementType::Vec2},
        DataBuffer(reinterpret_cast<u8 *>(obj_data.tex_coords.data()),
                   obj_data.tex_coords.size() * sizeof(math::Vec2)));
    mesh.indices = obj_data.indices;

    mesh.bounding_box = ComputeAABoundingBox(obj_data.positions.data(),
                                             obj_data.positions.size());

    auto cache_result =
        SerializeStaticMesh(mesh, path, model_cache_path_resolver);
    if (!cache_result.has_value()) {
      LOG_WARNING("Failed to write mesh cache: {}",
                  cache_result.error().message);
    }
  }
  return CreateMesh(path, std::move(mesh));
}

auto StaticMeshManager::CreateMesh(std::string_view name, StaticMesh &&mesh)
    -> std::expected<StaticMeshHandle, LuminaError> {
  auto static_mesh_handle = static_mesh_registry.Create(std::move(mesh));
  if (!static_mesh_handle.IsValid()) {
    return std::unexpected(
        LuminaError(std::format("Failed to create static mesh: {}", name)));
  }
  loaded_meshes[std::string(name)] = static_mesh_handle;
  return static_mesh_handle;
}

auto StaticMeshManager::Get(const StaticMeshHandle &handle)
    -> std::optional<const StaticMesh *> {
  return static_mesh_registry.Get(handle);
}

} // namespace lumina::core
