#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in flat uint frag_mode;

// Set 0, binding 0: font atlas (R8_UNORM for text, unused for solid color)
layout(set = 0, binding = 0) uniform sampler2D font_atlas;

layout(location = 0) out vec4 out_color;

void main() {
  if (frag_mode == 1u) {
    // Text: sample the R channel of the font atlas for glyph coverage
    float coverage = texture(font_atlas, frag_uv).r;
    out_color = vec4(frag_color.rgb, frag_color.a * coverage);
  } else {
    // Solid color rectangle (no texture sample)
    out_color = frag_color;
  }
}
