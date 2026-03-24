#pragma once

#include "lumina_types.hpp"
#include "math/vector.hpp"

#include <vector>
#include <vulkan/vulkan.h>

namespace lumina::renderer {

// Vertex format for all UI elements (screen-space pixel coords)
struct UIVertex {
  math::Vec2 position;  // screen-space pixels
  math::Vec2 uv;        // (0,0) for solid color, glyph UV for text
  math::Vec4 color;     // RGBA, normalized 0–1
  u32 mode;             // 0 = solid color, 1 = text (sample font atlas .r)
};

// A single Vulkan draw call within a UI batch, with an optional scissor rect
struct UIDrawCall {
  u32 index_offset = 0;
  u32 index_count = 0;
  bool has_scissor = false;
  VkRect2D scissor{};
};

// Per-frame data built from Clay render commands and consumed by UIRenderer
struct UIRenderBatch {
  std::vector<UIVertex> vertices;
  std::vector<u32> indices;
  std::vector<UIDrawCall> draw_calls;

  [[nodiscard]] auto IsEmpty() const -> bool { return vertices.empty(); }
  auto Reset() -> void {
    vertices.clear();
    indices.clear();
    draw_calls.clear();
  }
};

} // namespace lumina::renderer
