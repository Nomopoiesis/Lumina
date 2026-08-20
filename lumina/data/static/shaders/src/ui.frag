#version 450

#include "global/screen2d.global.glsl"

#include "stages/ui_2d.frag.glsl"

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
