#pragma once

#include "common/data_structures/data_buffer.hpp"
#include "core/static_mesh.hpp"
#include "vertex_layout.hpp"

namespace lumina::renderer {
using namespace lumina::common::data_structures;

auto SerializeVertexBuffer(const core::StaticMesh &mesh,
                           const VertexBufferLayout &layout)
    -> std::vector<std::pair<size_t, DataBuffer>>;

} // namespace lumina::renderer