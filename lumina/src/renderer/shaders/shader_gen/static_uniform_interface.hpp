#pragma once

#include "headers/scene3d.global.hpp"
#include "math/matrix.hpp"
#include "math/vector.hpp"

#include <span>

namespace lumina::renderer {

class FrameContext;

// Writes the Scene3D family's per-frame blocks into the frame's mapped uniform
// buffers. This half owns the generated struct layout, its padding, and which
// buffer each block belongs in. Gathering the values is the engine's job, which
// is why nothing here knows about World, entities or components — that is the
// same line DrawableProxyManager keeps.
//
// Re-exported rather than mirrored by a hand-written struct: a second
// declaration of the same GPU layout is exactly what the codegen exists to
// prevent.
using PointLight = shaders::scene3d::global::PointLight;

// The block holds a fixed array, so this is the cap. constexpr rather than a
// function so callers can size their own storage from it without allocating.
inline constexpr u32 kMaxPointLights =
    sizeof(shaders::scene3d::global::Lighting::point_lights) /
    sizeof(shaders::scene3d::global::PointLight);

auto WriteFrameGlobals(FrameContext &frame_context, const math::Mat4 &view,
                       const math::Mat4 &proj,
                       const math::Vec3 &camera_position) -> void;

// Lights past kMaxPointLights are dropped rather than overrunning the block.
auto WriteLighting(FrameContext &frame_context,
                   std::span<const PointLight> lights) -> void;

} // namespace lumina::renderer
