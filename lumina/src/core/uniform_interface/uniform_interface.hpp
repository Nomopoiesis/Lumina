#pragma once

namespace lumina::core {
class LuminaEngine;
} // namespace lumina::core

namespace lumina::renderer {

// Populates all per-frame uniform buffers for the current frame context.
// Declared as a friend of LuminaEngine so it can access engine internals
// directly without exposing additional public accessors.
auto UpdateFrameUniforms(core::LuminaEngine &engine) -> void;

} // namespace lumina::renderer
