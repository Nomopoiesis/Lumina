#include "vertex_serializer.hpp"

#include <algorithm>

namespace lumina::renderer {

auto SerializeVertexBuffer(const core::StaticMesh &mesh,
                           const VertexBufferLayout &layout)
    -> std::vector<std::pair<size_t, DataBuffer>> {
  std::vector<std::pair<size_t, DataBuffer>> vertex_buffers;
  for (const auto &stream : layout.streams) {
    size_t attribute_stride = 0;
    std::vector<const std::pair<core::VertexAttribute, std::vector<u8>> *>
        attribute_data;
    for (const auto &stream_attribute : stream.attributes) {
      auto it = std::ranges::find_if(
          mesh.vertex_attributes,
          [stream_attribute](const auto &attribute) -> bool {
            return attribute.first.type == stream_attribute.type;
          });
      ASSERT(it != mesh.vertex_attributes.end(),
             "Stream attribute not found in mesh vertex attributes");
      attribute_stride += core::GetElementTypeSize(it->first.element_type);
      attribute_data.push_back(&*it);
    }
    size_t size = mesh.vertex_count * attribute_stride;
    DataBuffer vertex_buffer(size);
    for (size_t i = 0; i < mesh.vertex_count; ++i) {
      size_t write_offset = i * attribute_stride;
      for (const auto *data : attribute_data) {
        const size_t elem_size =
            core::GetElementTypeSize(data->first.element_type);
        vertex_buffer.Write(write_offset,
                            data->second.data() + (i * elem_size), elem_size);
        write_offset += elem_size;
      }
    }
    vertex_buffers.emplace_back(attribute_stride, vertex_buffer);
  }
  return vertex_buffers;
}

} // namespace lumina::renderer