#include "font.hpp"

#include "common/data_structures/data_buffer.hpp"
#include "common/lumina_assert.hpp"
#include "common/lumina_util.hpp"
#include "common/path_registry.hpp"

#include "platform/platform_common/platform_services.hpp"

#include <vector>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace lumina::core {

auto CreateFont(const std::string &name, std::span<const i32> sizes)
    -> std::expected<Font, FontCreateError> {
  Font font;
  font.SetName(name);

  auto font_path = common::PathRegistry::Instance().fonts.Resolve(name);
  if (!std::filesystem::exists(font_path)) {
    return std::unexpected(
        FontCreateError{.message = "Font file not found: " + name});
  }

  auto file_handle =
      platform::common::PlatformServices::Instance().LuminaOpenFile(
          font_path.string().c_str());
  auto file_size =
      platform::common::PlatformServices::Instance().LuminaGetFileSize(
          file_handle);
  common::data_structures::DataBuffer data_buffer(file_size);
  platform::common::PlatformServices::Instance().LuminaReadFile(
      file_handle, data_buffer.Data(), file_size);
  platform::common::PlatformServices::Instance().LuminaCloseFile(file_handle);

  auto font_count = stbtt_GetNumberOfFonts(data_buffer.Data());
  ASSERT(font_count == 1, "Font file must contain exactly one font for now...");

  auto font_offset = stbtt_GetFontOffsetForIndex(data_buffer.Data(), 0);
  ASSERT(font_offset != -1, "Failed to get font offset");

  stbtt_fontinfo font_info;
  auto init_result =
      stbtt_InitFont(&font_info, data_buffer.Data(), font_offset);
  ASSERT(init_result != 0, "Failed to initialize font");

  auto atlas_size = 1024;

  for (const auto &size : sizes) {
    FontAtlas atlas = {
        .pixels = common::data_structures::DataBuffer(
            static_cast<size_t>(atlas_size * atlas_size * 1)),
        .width = atlas_size,
        .height = atlas_size,
    };
    stbtt_pack_context pack_context;
    stbtt_PackBegin(&pack_context, atlas.pixels.Data(), atlas_size, atlas_size,
                    0, 1, nullptr);
    stbtt_PackSetOversampling(&pack_context, 2, 2);
    stbtt_pack_range range{};
    range.font_size = static_cast<float>(size);
    range.first_unicode_codepoint_in_range = 32;
    range.num_chars = 95;
    stbtt_packedchar chardata[95];
    range.chardata_for_range = chardata;
    range.h_oversample = 2;
    range.v_oversample = 2;

    std::vector<stbrp_rect> rects(SafeI32ToU64(range.num_chars));
    // stbtt_PackFontRanges(&pack_context, data_buffer.Data(), 0, &range, 1);
    auto n = stbtt_PackFontRangesGatherRects(&pack_context, &font_info, &range,
                                             1, rects.data());
    ASSERT(n != 0, "Failed to gather font ranges");
    stbtt_PackFontRangesPackRects(&pack_context, rects.data(), n);
    ASSERT(pack_context.width != 0, "Failed to pack font ranges");
    i32 stb_result = stbtt_PackFontRangesRenderIntoRects(
        &pack_context, &font_info, &range, 1, rects.data());
    ASSERT(stb_result != 0, "Failed to render font ranges");
    stbtt_PackEnd(&pack_context);

    int stb_ascent = 0;
    int stb_descent = 0;
    int stb_line_gap = 0;
    stbtt_GetFontVMetrics(&font_info, &stb_ascent, &stb_descent, &stb_line_gap);
    const float scale =
        stbtt_ScaleForPixelHeight(&font_info, static_cast<float>(size));
    atlas.ascent = static_cast<f32>(stb_ascent) * scale;

    const auto inv_atlas_size = 1.0F / static_cast<f32>(atlas_size);

    for (i32 i = 0; i < range.num_chars; ++i) {
      const auto &packed_char_info = chardata[i];
      atlas.glyphs[range.first_unicode_codepoint_in_range + i] = GlyphInfo{
          .uv_top_left = math::Vec2{static_cast<f32>(packed_char_info.x0),
                                    static_cast<f32>(packed_char_info.y0)} *
                         inv_atlas_size,
          .uv_bottom_right = math::Vec2{static_cast<f32>(packed_char_info.x1),
                                        static_cast<f32>(packed_char_info.y1)} *
                             inv_atlas_size,
          .cursor_top_left_offset =
              math::Vec2{static_cast<f32>(packed_char_info.xoff),
                         static_cast<f32>(packed_char_info.yoff)},
          .cursor_bottom_right_offset =
              math::Vec2{static_cast<f32>(packed_char_info.xoff2),
                         static_cast<f32>(packed_char_info.yoff2)},
          .advance_x = packed_char_info.xadvance,
      };
    }

    font.AddAtlas(size, std::move(atlas));
  }
  return font;
}

auto Font::AddAtlas(i32 size, FontAtlas &&atlas) -> void {
  font_atalsses[size] = std::move(atlas);
}

} // namespace lumina::core
