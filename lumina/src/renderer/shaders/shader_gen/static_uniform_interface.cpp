#include "static_uniform_interface.hpp"

#include "renderer/frame_context.hpp"

#include <algorithm>
#include <cstring>

namespace lumina::renderer {

namespace g = lumina::shaders::scene3d::global;

auto WriteFrameGlobals(FrameContext &frame_context, const math::Mat4 &view,
                       const math::Mat4 &proj,
                       const math::Vec3 &camera_position) -> void {
  // Built whole and copied rather than written field by field through the
  // mapped pointer: the block is host-visible and write-combined, and a partial
  // write would leave the padding the generated struct carries undefined.
  g::FrameGlobals globals = {};
  globals.view = view;
  globals.proj = proj;
  globals.camera_position = camera_position;

  std::memcpy(
      frame_context.GetShaderFamilyScene3DResources().frame_globals.mapped,
      &globals, sizeof(g::FrameGlobals));
}

auto WriteLighting(FrameContext &frame_context,
                   std::span<const PointLight> lights) -> void {
  g::Lighting lighting = {};

  // Clamped here as well as at the gather. The caller sizes its storage from
  // kMaxPointLights, so this only fires if someone hands over a span from
  // elsewhere — at which point silently dropping beats writing past the array.
  const auto count =
      static_cast<u32>(std::min<size_t>(lights.size(), kMaxPointLights));
  std::copy_n(lights.begin(), count, std::begin(lighting.point_lights));
  lighting.point_light_count = static_cast<int32_t>(count);

  std::memcpy(frame_context.GetShaderFamilyScene3DResources().lighting.mapped,
              &lighting, sizeof(g::Lighting));
}

} // namespace lumina::renderer
