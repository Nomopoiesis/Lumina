#include "obj_parser.hpp"

#include "math/vector.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <charconv>

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

  u16 first_index = 0;
  u16 prev_index = 0;
  u32 corner = 0;

  while (curr < end) {
    // The bounds test comes first on purpose: the other order dereferences
    // working before establishing that it is still inside the line.
    while ((working < end) && (*working != ' ')) {
      ++working;
    }

    auto face_indicies = ParseFaceVert(curr, working);
    auto [it, inserted] = mesh_data.vertex_dedup_cache.emplace(
        face_indicies, static_cast<u16>(mesh_data.out_positions.size()));

    if (inserted) {
      mesh_data.out_positions.push_back(
          mesh_data.raw_positions[face_indicies.position_index]);
      mesh_data.out_normals.push_back(
          mesh_data.raw_normals[face_indicies.normal_index]);
      mesh_data.out_tex_coords.push_back(
          face_indicies.tex_coord_index < mesh_data.raw_tex_coords.size()
              ? mesh_data.raw_tex_coords[face_indicies.tex_coord_index]
              : math::Vec2{});
    }

    const u16 index = it->second;
    if (corner == 0) {
      first_index = index;
    } else if (corner >= 2) {
      mesh_data.indices.push_back(first_index);
      mesh_data.indices.push_back(prev_index);
      mesh_data.indices.push_back(index);
    }
    prev_index = index;
    ++corner;

    curr = working + 1;
    working = curr;
  }
}

// OBJ does not require an object declaration. Plenty of exporters emit bare
// vertex and face data, and data/runtime/models/suzanne.obj is one of them:
// 1529 lines with no 'o' and no 'g'. Naming the implicit object keeps such files
// loadable instead of tripping the assertion that used to guard this.
//
// The name is a counter rather than a random value on purpose. Nothing
// downstream ever reads it - OBJ_Result carries geometry only, and ParseOBJ
// hands back objects.begin() - so randomness would add no uniqueness the
// counter does not already provide, while making two parses of one file
// disagree in a debugger or a cache key.
//
// current_mesh_name is a view, so it is aimed at the key stored inside the map.
// unordered_map is node-based, so that reference stays valid as the map grows;
// a view of a local string would dangle the moment this returned.
auto EnsureCurrentMesh(ParseContext &parse_context) -> MeshData & {
  if (!parse_context.current_mesh_name.empty()) {
    return parse_context.objects[std::string(parse_context.current_mesh_name)];
  }

  auto it = parse_context.objects
                .try_emplace("unnamed_" +
                             std::to_string(parse_context.objects.size()))
                .first;
  parse_context.current_mesh_name = it->first;
  return it->second;
}

auto ParseLine(const std::string_view &line, ParseContext &parse_context)
    -> void {
  // Splitting on '\n' yields an empty view for a blank line, and OBJ files use
  // blank lines as separators - suzanne.obj has one at line 1027 and one at the
  // end. Returning here rather than falling into the switch matters because
  // line[0] on an empty view reads out of bounds before any case is chosen.
  if (line.empty()) {
    return;
  }

  switch (line[0]) {
    case 'o': {
      parse_context.current_mesh_name = line.substr(2);
      parse_context.objects.emplace(parse_context.current_mesh_name,
                                    MeshData());
    } break;
    case 'v': {
      auto &mesh_data = EnsureCurrentMesh(parse_context);
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
      auto &mesh_data = EnsureCurrentMesh(parse_context);
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
  if (data.IsEmpty()) {
    return result;
  }

  const auto *text = &data.As<char>();
  const auto *end = text + data.Size();
  ParseContext parse_context;
  while (text < end) {
    const auto *line_end = std::find(text, end, '\n');
    const auto line = std::string_view(text, line_end);
    ParseLine(line, parse_context);
    text = line_end + 1;
  }

  // A malformed file can parse without ever declaring an object; returning the
  // empty result beats dereferencing objects.begin() on an empty map.
  if (parse_context.objects.empty()) {
    return result;
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
