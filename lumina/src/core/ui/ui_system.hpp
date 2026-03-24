#pragma once

#include "common/data_structures/data_buffer.hpp"
#include "core/font.hpp"
#include "lumina_types.hpp"

#include <unordered_map>

// clay.h declarations only (no CLAY_IMPLEMENTATION here)
#include "clay.h"

namespace lumina::core {

class UISystem {
public:
  auto Initialize(u32 width, u32 height) -> void;
  auto Shutdown() -> void;

  // Call at the start of each frame (typically from LuminaEngine::BeginFrame).
  // Resets Clay and sets the current layout dimensions.
  auto BeginLayout(u32 width, u32 height) -> void;

  // Call after all CLAY_* macros for the frame have been issued.
  // Returns the array of render commands to be consumed by the engine.
  auto EndLayout() -> Clay_RenderCommandArray;

  // Register a font so Clay text commands can resolve glyph metrics.
  // fontId must match the fontId used in CLAY_TEXT_CONFIG.
  auto RegisterFont(u16 font_id, Font *font) -> void;

  [[nodiscard]] auto GetFont(u16 font_id) const -> Font *;

private:
  static auto MeasureTextCallback(Clay_StringSlice text,
                                  Clay_TextElementConfig *config,
                                  void *user_data) -> Clay_Dimensions;

  std::unordered_map<u16, Font *> fonts;
  // Backing memory for Clay's internal arena
  common::data_structures::DataBuffer arena_memory;
};

} // namespace lumina::core
