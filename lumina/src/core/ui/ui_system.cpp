#include "ui_system.hpp"

#include "common/logger/logger.hpp"

#define CLAY_IMPLEMENTATION
#include "clay.h"

namespace lumina::core {

static auto ClayErrorHandler(Clay_ErrorData error) -> void {
  LOG_ERROR("Clay error: {}", error.errorText.chars);
}

auto UISystem::Initialize(u32 width, u32 height) -> void {
  const auto min_size = Clay_MinMemorySize();
  arena_memory = common::data_structures::DataBuffer(min_size);
  auto arena = Clay_CreateArenaWithCapacityAndMemory(
      min_size, static_cast<void *>(arena_memory.Data()));
  Clay_Initialize(arena,
                  Clay_Dimensions{.width = static_cast<float>(width),
                                  .height = static_cast<float>(height)},
                  Clay_ErrorHandler{.errorHandlerFunction = ClayErrorHandler,
                                    .userData = nullptr});
  Clay_SetMeasureTextFunction(MeasureTextCallback, this);
}

auto UISystem::Shutdown() -> void { arena_memory.Reset(); }

auto UISystem::BeginLayout(u32 width, u32 height) -> void {
  Clay_SetLayoutDimensions(
      Clay_Dimensions{.width = static_cast<float>(width),
                      .height = static_cast<float>(height)});
  Clay_BeginLayout();
}

auto UISystem::EndLayout() -> Clay_RenderCommandArray {
  return Clay_EndLayout();
}

auto UISystem::RegisterFont(u16 font_id, Font *font) -> void {
  fonts[font_id] = font;
}

auto UISystem::GetFont(u16 font_id) const -> Font * {
  auto it = fonts.find(font_id);
  return it != fonts.end() ? it->second : nullptr;
}

auto UISystem::MeasureTextCallback(Clay_StringSlice text,
                                   Clay_TextElementConfig *config,
                                   void *user_data) -> Clay_Dimensions {
  auto *system = static_cast<UISystem *>(user_data);
  auto *font = system->GetFont(config->fontId);
  if (font == nullptr) {
    return {.width = 0, .height = 0};
  }

  // Use the requested font size; fall back to the closest available atlas
  auto *atlas = font->GetAtlas(static_cast<i32>(config->fontSize));
  if (atlas == nullptr) {
    return {.width = 0, .height = 0};
  }

  float width = 0.0F;
  for (i32 i = 0; i < text.length; ++i) {
    const auto codepoint =
        static_cast<i32>(static_cast<unsigned char>(text.chars[i]));
    auto it = atlas->glyphs.find(codepoint);
    if (it != atlas->glyphs.end()) {
      width += it->second.advance_x;
      if (i < text.length - 1) {
        width += static_cast<float>(config->letterSpacing);
      }
    }
  }

  const float height = config->lineHeight > 0
                           ? static_cast<float>(config->lineHeight)
                           : static_cast<float>(config->fontSize);
  return {.width = width, .height = height};
}

} // namespace lumina::core
