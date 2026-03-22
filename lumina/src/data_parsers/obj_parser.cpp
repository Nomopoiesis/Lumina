#include "obj_parser.hpp"

#include "math/vector.hpp"

#include <unordered_map>
#include <vector>

namespace lumina::data_parsers {

namespace {

struct FaceIndicies {
  u16 position_index = 0;
  u16 normal_index = 0;
  u16 tex_coord_index = 0;

  auto operator==(const FaceIndicies &other) const -> bool = default;
};

struct FaceIndiciesHash {
  auto operator()(const FaceIndicies &v) const -> size_t {
    size_t h = v.position_index;
    h = (h * 2654435761U) ^ v.tex_coord_index;
    h = (h * 2654435761U) ^ v.normal_index;
    return h;
  }
};

struct MeshData {
  std::vector<math::Vec3> raw_positions;
  std::vector<math::Vec3> raw_normals;
  std::vector<math::Vec2> raw_tex_coords;

  std::vector<math::Vec3> out_positions;
  std::vector<math::Vec3> out_normals;
  std::vector<math::Vec2> out_tex_coords;
  std::vector<u16> indices;

  std::unordered_map<FaceIndicies, u16, FaceIndiciesHash> vertex_dedup_cache;
};

struct ParseContext {
  std::unordered_map<std::string, MeshData> objects;
  std::string_view current_mesh_name;
};

template <math::LuminaVectorType T>
auto ParseFloats(const std::string_view &line, std::vector<T> &values) -> void {
  const char *curr = line.data();
  const char *end = line.data() + line.size();
  const char *working = curr;
  T value;
  auto *curr_scalar = value.DataPtr();
  while (curr < end) {
    if ((std::isdigit(*working) != 0) || *working == '.' || *working == '-') {
      working++;
    } else {
      std::from_chars(curr, working, *curr_scalar);
      curr_scalar++;
      curr = working + 1;
      working = curr;
    }
  }
  values.push_back(value);
}

auto ParseFaceVert(const char *&begin, const char *&end) -> FaceIndicies {
  FaceIndicies face_indicies;
  auto [p, ec1] = std::from_chars(begin, end, face_indicies.position_index);
  face_indicies.position_index--;
  begin = p;
  if (begin < end && *begin == '/') {
    begin++;
    if (begin < end && *begin != '/') {
      auto [q, ec2] =
          std::from_chars(begin, end, face_indicies.tex_coord_index);
      face_indicies.tex_coord_index--;
      begin = q;
    }
    if (begin < end && *begin == '/') {
      begin++;
      auto [r, ec3] = std::from_chars(begin, end, face_indicies.normal_index);
      face_indicies.normal_index--;
      begin = r;
    }
  }
  return face_indicies;
}

auto ParseFace(const std::string_view &line, MeshData &mesh_data) -> void {
  const char *curr = line.data();
  const char *end = line.data() + line.size();
  const char *working = curr;
  u8 vertex_count = 0;
  while (curr < end) {
    while ((*working != ' ') && (working < end)) {
      ++working;
    }

    ASSERT(vertex_count < 3, "Face must have no more than 3 vertices");
    ++vertex_count;
    auto face_indicies = ParseFaceVert(curr, working);
    auto [it, inserted] = mesh_data.vertex_dedup_cache.emplace(
        face_indicies, static_cast<u16>(mesh_data.out_positions.size()));

    if (inserted) {
      mesh_data.out_positions.push_back(
          mesh_data.raw_positions[face_indicies.position_index]);
      mesh_data.out_normals.push_back(
          mesh_data.raw_normals[face_indicies.normal_index]);
      mesh_data.out_tex_coords.push_back(
          mesh_data.raw_tex_coords[face_indicies.tex_coord_index]);
    }
    mesh_data.indices.push_back(it->second);

    curr = working + 1;
    working = curr;
  }
}

auto ParseLine(const std::string_view &line, ParseContext &parse_context)
    -> void {
  switch (line[0]) {
    case 'o': {
      parse_context.current_mesh_name = line.substr(2);
      parse_context.objects.emplace(parse_context.current_mesh_name,
                                    MeshData());
    } break;
    case 'v': {
      ASSERT(!parse_context.current_mesh_name.empty(),
             "Current mesh name is not set");
      auto &mesh_data =
          parse_context.objects[std::string(parse_context.current_mesh_name)];
      if (line[1] == ' ') {
        ParseFloats<math::Vec3>(line.substr(2), mesh_data.raw_positions);
      } else if (line[1] == 'n') {
        ParseFloats<math::Vec3>(line.substr(3), mesh_data.raw_normals);
      } else if (line[1] == 't') {
        ParseFloats<math::Vec2>(line.substr(3), mesh_data.raw_tex_coords);
      } else {
        ASSERT(false, "Unknown vertex position type");
      }
    } break;
    case 'f': {
      auto &mesh_data =
          parse_context.objects[std::string(parse_context.current_mesh_name)];
      ParseFace(line.substr(2), mesh_data);
    } break;
    case '#':
    case 'm':
    case 's': {
      // Skip comment
      // Skip material
      // Skip smoothing group
    } break;
    default: {
      ASSERT(false, "Unknown line type");
    } break;
  }
}

} // namespace

auto ParseOBJ(const DataBufferView &data) -> OBJ_Result {
  OBJ_Result result;
  const auto *text = &data.As<char>();
  const auto *end = text + data.Size();
  ParseContext parse_context;
  while (text < end) {
    const auto *line_end = std::find(text, end, '\n');
    const auto line_length = line_end - text;
    const auto line = std::string_view(text, line_length);
    ParseLine(line, parse_context);
    text = line_end + 1;
  }

  auto &mesh_data = parse_context.objects.begin()->second;
  result.vertex_count = mesh_data.out_positions.size();
  result.positions = mesh_data.out_positions;
  result.normals = mesh_data.out_normals;
  result.tex_coords = mesh_data.out_tex_coords;
  result.indices = mesh_data.indices;
  return result;
}
} // namespace lumina::data_parsers