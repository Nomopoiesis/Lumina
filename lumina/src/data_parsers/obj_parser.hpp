#pragma once

#include "common/data_structures/data_buffer.hpp"
#include "math/vector.hpp"

#include <vector>

namespace lumina::data_parsers {

using namespace lumina::common::data_structures;

struct OBJ_Result {
  size_t vertex_count;
  std::vector<math::Vec3> positions;
  std::vector<math::Vec3> normals;
  std::vector<math::Vec2> tex_coords;
  std::vector<u16> indices;
};

auto ParseOBJ(const DataBufferView &data) -> OBJ_Result;

} // namespace lumina::data_parsers