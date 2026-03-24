#pragma once

#include "lumina_types.hpp"

#include "common/data_structures/data_buffer.hpp"
#include "math/vector.hpp"
#include "texture.hpp"

#include <span>
#include <string>
#include <unordered_map>

#include <expected>

namespace lumina::core {

struct FontCreateError {
  std::string message;
};

struct GlyphInfo {
  math::Vec2 uv_top_left;                // normalized UV top-left
  math::Vec2 uv_bottom_right;            // normalized UV bottom-right
  math::Vec2 cursor_top_left_offset;     // pixel offset from cursor to glyph
                                         // top-left
  math::Vec2 cursor_bottom_right_offset; // pixel offset from cursor to glyph
                                         // bottom-right
  f32 advance_x{};                       // horizontal advance in pixels
};

struct FontAtlas {
  common::data_structures::DataBuffer pixels;
  i32 width;
  i32 height;
  f32 ascent; // scaled ascent in pixels (baseline offset from top of line box)
  std::unordered_map<i32, GlyphInfo> glyphs; // codepoint → glyph
  TextureHandle texture_handle;
};

class Font {
public:
  Font() noexcept = default;
  Font(const Font &other) noexcept = default;
  Font(Font &&other) noexcept = default;
  auto operator=(const Font &other) noexcept -> Font & = default;
  auto operator=(Font &&other) noexcept -> Font & = default;
  ~Font() noexcept = default;

  auto AddAtlas(i32 size, FontAtlas &&atlas) -> void;
  auto SetName(const std::string &n) -> void { name = n; }

  [[nodiscard]] auto GetAtlas(i32 size) -> FontAtlas * {
    auto it = font_atalsses.find(size);
    return it != font_atalsses.end() ? &it->second : nullptr;
  }

private:
  std::unordered_map<i32, FontAtlas> font_atalsses;
  std::string name;
};

auto CreateFont(const std::string &name, std::span<const i32> sizes)
    -> std::expected<Font, FontCreateError>;

} // namespace lumina::core
